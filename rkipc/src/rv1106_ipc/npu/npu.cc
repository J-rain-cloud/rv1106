#include "npu.h"

#include "../include/rk_comm_video.h"
#include "RgaApi.h"
#include <vector>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static network_info_t network;
static rknn_tensor_type input_type = RKNN_TENSOR_UINT8;
static rknn_tensor_format input_layout = RKNN_TENSOR_NHWC;
static rknn_context ctx = 0;
static int ret = 0;

static bool g_npu_run = true;
static bool g_recv_data = false;
static bool g_process_end = true;
static pthread_mutex_t g_network_lock = PTHREAD_MUTEX_INITIALIZER;
static void *g_input_data = NULL;
static int g_fd;
static int g_img_width = 0;
static int g_img_height = 0;
static int enPixelFormat;

static pthread_t g_npu_proc;

/*-------------------------------------------
                  Functions
-------------------------------------------*/
static void dump_tensor_attr(rknn_tensor_attr *attr) {
	char dims[128] = {0};
	for (int i = 0; i < attr->n_dims; ++i) {
		int idx = strlen(dims);
		sprintf(&dims[idx], "%d%s", attr->dims[i], (i == attr->n_dims - 1) ? "" : ", ");
	}
	printf("  index=%d, name=%s, n_dims=%d, dims=[%s], n_elems=%d, size=%d, fmt=%s, type=%s, "
	       "qnt_type=%s, "
	       "zp=%d, scale=%f\n",
	       attr->index, attr->name, attr->n_dims, dims, attr->n_elems, attr->size,
	       get_format_string(attr->fmt), get_type_string(attr->type),
	       get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

static int NC1HWC2_to_NCHW(const int8_t *src, int8_t *dst, uint32_t *dims, int channel,
                           int hw_dst) {
	int batch = dims[0];
	int C1 = dims[1];
	int h = dims[2];
	int w = dims[3];
	int C2 = dims[4];
	int hw_src = w * h;
	for (int i = 0; i < batch; i++) {
		src = src + i * C1 * hw_src * C2;
		dst = dst + i * channel * hw_dst;
		for (int c = 0; c < channel; ++c) {
			int plane = c / C2;
			const int8_t *src_c = plane * hw_src * C2 + src;
			int offset = c % C2;
			for (int cur_h = 0; cur_h < h; ++cur_h)
				for (int cur_w = 0; cur_w < w; ++cur_w) {
					int cur_hw = cur_h * w + cur_w;
					dst[c * hw_dst + cur_h * w + cur_w] = src_c[C2 * cur_hw + offset];
				}
		}
	}

	return 0;
}

static int storage_pic(void * src)
{
	FILE *fp;
	fp = fopen("input_picture", "wb+");
	if(fwrite(src, g_img_width*g_img_height*1.5, 1, fp) <= 0)
	{
		printf("--------output image failed--------\n");
		return -1;
	}
	printf("--------output image success--------\n");
	fclose(fp);
	return 0;
}

// network init
int network_init(char *model_path) {
	// load model
	ret = rknn_init(&ctx, model_path, 0, 0, NULL);
	if (ret < 0) {
		printf("--------rknn_init fail! ret=%d--------\n", ret);
		return -1;
	}

	// sdk version
	rknn_sdk_version sdk_version;
	ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &sdk_version, sizeof(sdk_version));
	if (ret != RKNN_SUCC) {
		printf("rknn_query version fail! ret=%d\n", ret);
		return -1;
	}
	printf("rknn_api/rknnrt version: %s, driver version: %s\n", sdk_version.api_version,
	       sdk_version.drv_version);

	// input output num
	ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &network.io_num, sizeof(network.io_num));
	if (ret != RKNN_SUCC) {
		printf("rknn_query in_out_num fail! ret=%d\n", ret);
		return -1;
	}
	printf("model input num: %d, output num: %d\n", network.io_num.n_input,
	       network.io_num.n_output);

	// input tensor
	printf("input tensors:\n");
	network.input_attrs =
	    (rknn_tensor_attr *)malloc(sizeof(rknn_tensor_attr) * network.io_num.n_input);
	memset(network.input_attrs, 0, network.io_num.n_input * sizeof(rknn_tensor_attr));
	printf("----------------------------->>\n");
	for (uint32_t i = 0; i < network.io_num.n_input; i++) {
		network.input_attrs[i].index = i;
		// query info
		ret = rknn_query(ctx, RKNN_QUERY_NATIVE_INPUT_ATTR, &(network.input_attrs[i]),
		                 sizeof(rknn_tensor_attr));
		if (ret < 0) {
			printf("rknn_query input error! ret=%d\n", ret);
			return -1;
		}
		dump_tensor_attr(&network.input_attrs[i]);
	}

	// output tensor
	printf("output tensors:\n");
	network.output_attrs =
	    (rknn_tensor_attr *)malloc(sizeof(rknn_tensor_attr) * network.io_num.n_output);
	memset(network.output_attrs, 0, network.io_num.n_output * sizeof(rknn_tensor_attr));
	printf("----------------------------->>\n");
	for (uint32_t i = 0; i < network.io_num.n_output; i++) {
		network.output_attrs[i].index = i;
		// query info
		ret = rknn_query(ctx, RKNN_QUERY_NATIVE_OUTPUT_ATTR, &(network.output_attrs[i]),
		                 sizeof(rknn_tensor_attr));
		if (ret != RKNN_SUCC) {
			printf("rknn_query output error! ret=%d\n", ret);
			return -1;
		}
		dump_tensor_attr(&network.output_attrs[i]);
	}

	// Get custom string
	ret = rknn_query(ctx, RKNN_QUERY_CUSTOM_STRING, &network.custom_string,
	                 sizeof(network.custom_string));
	if (ret != RKNN_SUCC) {
		printf("rknn_query fail! ret=%d\n", ret);
		return -1;
	}
	printf("custom string: %s\n", network.custom_string.string);

	//??????????????????
	network.input_attrs[0].type = input_type;
	network.input_attrs[0].fmt =
	    input_layout; // default fmt is NHWC, npu only support NHWC in zero copy mode

	printf("--------network init over--------\n");

	//???????????????????????????
	if (network.input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
		printf("model is NCHW input fmt\n");
		network.channel = network.input_attrs[0].dims[1];
		network.width = network.input_attrs[0].dims[2];
		network.height = network.input_attrs[0].dims[3];
	} else {
		printf("model is NHWC input fmt\n");
		network.channel = network.input_attrs[0].dims[3];
		network.width = network.input_attrs[0].dims[1];
		network.height = network.input_attrs[0].dims[2];
	}

	//??????rga????????????????????????
	network.param.width = network.width;
	network.param.height = network.height;
	network.param.format = RK_FORMAT_RGB_888;
	printf("--------configure param over--------\n");

	//????????????????????????
	network.buf = malloc(sizeof(unsigned char) * network.width * network.height * network.channel);
	memset(network.buf, 0,
	       sizeof(unsigned char) * (network.height * network.width * network.channel));

	// ??????NPU????????????
	pthread_create(&g_npu_proc, NULL, npu_process, (void *)&ctx);
	return 0;
}

// network deinit
void network_exit() {
	g_npu_run = false;

	if (g_npu_proc) {
		pthread_join(g_npu_proc, NULL);
		g_npu_proc = 0;
	}
	printf("--------network deinit--------\n");

	rknn_destroy(ctx);

	if (network.buf)
		free(network.buf);

	if (network.input_mems[0]) {
		rknn_destroy_mem(ctx, network.input_mems[0]);
	}
	if (network.output_mems[0]) {
		rknn_destroy_mem(ctx, network.output_mems[0]);
		rknn_destroy_mem(ctx, network.output_mems[1]);
		rknn_destroy_mem(ctx, network.output_mems[2]);
	}

	if (g_input_data)
		free(g_input_data);

	printf("--------network deinit over--------\n");
}

static int get_rga_format(int src_format) {
	int dst_format;
	switch (src_format) {
	case RK_FMT_YUV420SP:
		dst_format = RK_FORMAT_YCbCr_420_SP;
		break;
	case RK_FMT_RGB888:
		dst_format = RK_FORMAT_RGB_888;
		break;
	case RK_FMT_BGR888:
		dst_format = RK_FORMAT_BGR_888;
		break;
	default:
		dst_format = RK_FORMAT_RGB_888;
		break;
	}
	return dst_format;
}

static char *get_rga_format_string(int format, char *str) {
	switch (format) {
	case RK_FORMAT_YCbCr_420_SP:
		strcpy(str, "RK_FORMAT_YCbCr_420_SP");
		break;
	case RK_FORMAT_RGB_888:
		strcpy(str, "RK_FORMAT_RGB_888");
		break;
	case RK_FORMAT_BGR_888:
		strcpy(str, "RK_FORMAT_BGR_888");
		break;
	default:
		strcpy(str, "none");
		break;
	}

	return str;
}

void recv_frame(void *ptr, int width, int height, int format, int fd) {
	char str[128];
	memset(str, 0, 128);

	pthread_mutex_lock(&g_network_lock);
	if (g_process_end == false) {
		pthread_mutex_unlock(&g_network_lock);
		printf("-------- npu processing --------\n");
		return;
	}

	g_recv_data = true;
	g_process_end = false;

	g_img_width = width;
	g_img_height = height;
	g_fd = fd;
	//????????????
	ret = storage_pic(ptr);
	if(ret != 0 )
	{
		printf("--------output image failed--------\n");
		return ;
	}
	//????????????????????????
	g_input_data = malloc(width * height * network.channel/2);
	memset(g_input_data, 0, width * height * network.channel/2);
	memcpy(g_input_data, ptr, (width * height * network.channel)/2);

	printf("--------input image frame's width=%d, height=%d--------\n", g_img_width, g_img_height);
	printf("--------input image format=%s\n", get_rga_format_string(get_rga_format(format), str));

	// resize(ptr, width, height, get_rga_format(format),
	//	network.buf, network.width, network.height, RK_FORMAT_RGB_888, 0);

	pthread_mutex_unlock(&g_network_lock);
	return;
}

// npu process
void *npu_process(void *arg) {

	printf("--------%s thread start--------\n", __func__);
	rknn_context *ctx = (rknn_context *)arg;
	int ret = 0;

	do { // ?????????????????????,?????????????????????
		pthread_mutex_lock(&g_network_lock);
		//?????????????????????????????????  input_data
		if (g_recv_data == false) {
			pthread_mutex_unlock(&g_network_lock);
			continue;
		}
		//???????????????????????????
		printf("--------recv data success! will process it--------\n");
		
		
		//????????????
		network.scale_w = (float)network.width / g_img_width;
		network.scale_h = (float)network.height / g_img_height;
		printf("--------model_width=%d, model_height=%d, input_width=%d, input_height=%d--------\n",
		       network.width, network.height, g_img_width, g_img_height);

		//?????????????????????RGA????????????
		/*rga_buffer_handle_t src_rga_buffer_handle;
		im_handle_param_t param;
		param.width = g_img_width;
		param.height = g_img_height;
		param.format = RK_FORMAT_YCbCr_420_SP;
		//src_rga_buffer_handle = importbuffer_virtualaddr(g_input_data, &param);
		src_rga_buffer_handle = importbuffer_fd(g_fd, &param);
		rga_buffer_handle_t dst_rga_buffer_handle;
		dst_rga_buffer_handle = importbuffer_virtualaddr(network.buf, &network.param);
		printf("-------- reflect virtual address over--------\n");*/

		rga_buffer_t src_rga_handle;
		rga_buffer_t dst_rga_handle;
		im_rect src_rect;
    	im_rect dst_rect;
    	memset(&src_rect, 0, sizeof(src_rect));
    	memset(&dst_rect, 0, sizeof(dst_rect));
    	memset(&src_rga_handle, 0, sizeof(src_rga_handle));
    	memset(&dst_rga_handle, 0, sizeof(dst_rga_handle));
		// ???????????????????????? rga_buffer_t
		printf("--------g_img_width=%d, g_img_height=%d--------\n",g_img_width, g_img_height);
		
		src_rga_handle = wrapbuffer_virtualaddr(g_input_data, g_img_width, g_img_height,
		                                   RK_FORMAT_YCbCr_420_SP); 
		dst_rga_handle = wrapbuffer_virtualaddr(network.buf, network.width, network.height,
		                                   RK_FORMAT_RGB_888); 

		printf("-------- transfer image params to rga_buffer_t over--------\n");
		
		ret = imcheck(src_rga_handle, dst_rga_handle, src_rect, dst_rect);
    	if (IM_STATUS_NOERROR != ret)
    	{
        	printf("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
        	pthread_exit((void *)-1);
    	}
		printf("--------check success--------\n");
		//?????????resize????????????????????????640*640
		printf("--------scale_w=%f, scale_h=%f--------\n", network.scale_w, network.scale_h);
		//ret = imresize(src_rga_handle, dst_rga_handle);
		printf("-------- width=%d,\twstride=%d,\tprt=%p,\theight=%d,\thstride=%d--------\n",
		       src_rga_handle.width, src_rga_handle.wstride, src_rga_handle.vir_addr,
		       src_rga_handle.height, src_rga_handle.hstride);
		printf("-------- width=%d,\twstride=%d,\tprt=%p,\theight=%d,\thstride=%d--------\n",
		       dst_rga_handle.width, dst_rga_handle.wstride, dst_rga_handle.vir_addr,
		       dst_rga_handle.height, dst_rga_handle.hstride);
		/*if (ret != IM_STATUS_SUCCESS) {
			printf("-------- resize image failed--------ret=%d\n", ret);
			pthread_exit((void *)-1);
		}*/
		printf("-------- resize image over--------\n");


		// ??????????????????
		network.input_mems[0] = rknn_create_mem(*ctx, network.input_attrs[0].size_with_stride);
		if (network.input_mems[0] == NULL) {
			printf("--------allocate input memory failed--------\n");
			pthread_exit((void *)-1);
		}

		//????????????????????????????????????????????????
		int w_stride = network.input_attrs[0].w_stride;

		printf("--------copy data to input memory--------\n");
		if (w_stride == network.width) //????????????????????????
		{
			printf("--------0 pixel fill--------\n");
			// ???????????? dst src size=H*W*C
			if (network.input_mems[0]->virt_addr) {
				printf("--------memory copy--------\n");
				memcpy(network.input_mems[0]->virt_addr, network.buf,
				       network.width * network.height * network.channel);
			} else
				printf("--------address error--------\n");

		} else // ???????????????????????????
		{
			printf("--------pixel fill--------\n");
			// copy from src to dst with w_stride
			uint8_t *src_ptr = (uint8_t *)network.buf;
			uint8_t *dst_ptr = (uint8_t *)network.input_mems[0]->virt_addr;
			// width-channel elements
			int src_wc_elems = network.width * network.channel;
			int dst_wc_elems = w_stride * network.channel;
			for (int h = 0; h < network.height; ++h) {
				memcpy(dst_ptr, src_ptr, src_wc_elems);
				src_ptr += src_wc_elems;
				dst_ptr += dst_wc_elems; // ???????????????????????????,??????????????????0,?????????*??????????????????
			}
		}
		printf("--------copy over--------\n");


		// ??????????????????
		for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
			network.output_mems[i] = rknn_create_mem(*ctx, network.output_attrs[i].size_with_stride);
			if (network.output_mems[i] == NULL) {
				printf("--------allocate output memory failed--------\n");
				pthread_exit((void *)-1);
			}
		}

		// ??????????????????????????????
		ret = rknn_set_io_mem(*ctx, network.input_mems[0], &network.input_attrs[0]);
		if (ret < 0) {
			printf("input_memory: rknn_set_io_mem fail! ret=%d\n", ret);
			pthread_exit((void *)-1);
		}

		// ??????????????????????????????
		for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
			// set output memory and attribute
			ret = rknn_set_io_mem(*ctx, network.output_mems[i], &network.output_attrs[i]);
			if (ret < 0) {
				printf("output_memory: rknn_set_io_mem fail! ret=%d\n", ret);
				pthread_exit((void *)-1);
			}
		}
		printf("--------set in_out put struct info over--------\n");


		// Run
		printf("--------model start run--------\n");
		ret = rknn_run(*ctx, NULL);
		if (ret < 0) {
			printf("rknn run error %d\n", ret);
			pthread_exit((void *)-1);
		}

		// ?????????????????????????????????
		printf("output origin tensors:\n");
		rknn_tensor_attr orig_output_attrs[network.io_num.n_output];
		printf("----------------------------->>\n");
		memset(orig_output_attrs, 0, network.io_num.n_output * sizeof(rknn_tensor_attr));
		for (uint32_t i = 0; i < network.io_num.n_output; i++) {
			orig_output_attrs[i].index = i;
			// query info
			ret = rknn_query(*ctx, RKNN_QUERY_OUTPUT_ATTR, &(orig_output_attrs[i]),
			                 sizeof(rknn_tensor_attr));
			if (ret != RKNN_SUCC) {
				printf("rknn_query fail! ret=%d\n", ret);
				pthread_exit((void *)-1);
			}
			dump_tensor_attr(&orig_output_attrs[i]);
		}
		printf("--------query origin output tensor success--------\n");
		// ????????????NCHW??????????????????
		rknn_tensor_mem *output_mems_nchw[network.io_num.n_output];
		for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
			int size = orig_output_attrs[i].size_with_stride;
			output_mems_nchw[i] = rknn_create_mem(*ctx, size);
		}
		printf("--------allocate origin output NHCW memory over--------\n");

		for (uint32_t i = 0; i < network.io_num.n_output; i++) {
			int channel = orig_output_attrs[i].dims[1];
			int h = orig_output_attrs[i].n_dims > 2 ? orig_output_attrs[i].dims[2] : 1;
			int w = orig_output_attrs[i].n_dims > 3 ? orig_output_attrs[i].dims[3] : 1;
			int hw = h * w;
			// ???????????????NCHW???????????????????????????????????????  ??????????????????NC1HWC2,???????????????NCHW
			ret = NC1HWC2_to_NCHW((int8_t *)network.output_mems[i]->virt_addr,
			                      (int8_t *)output_mems_nchw[i]->virt_addr,
			                      network.output_attrs[i].dims, channel, hw);
			if (ret != 0) {
				printf("--------transfer NC1HWC2 to NCHW failed--------\n");
			}
		}

		printf("--------start post process--------\n");
		// post process
		for (int i = 0; i < network.io_num.n_output; ++i) {
			network.out_scales.push_back(network.output_attrs[i].scale);
			network.out_zps.push_back(network.output_attrs[i].zp);
		}

		ret = post_process(
		    (int8_t *)output_mems_nchw[0]->virt_addr, (int8_t *)output_mems_nchw[1]->virt_addr,
		    (int8_t *)output_mems_nchw[2]->virt_addr, 640, 640, network.box_conf_threshold,
		    network.nms_threshold, network.scale_w, network.scale_h, network.out_zps,
		    network.out_scales, &network.detect_result_group);
		if (ret != 0) {
			printf("--------post process failed--------\n");
		}

		printf("--------post process over, and then printf the result box info--------\n");
		char text[256];
		for (int i = 0; i < network.detect_result_group.count; i++) {
			detect_result_t *det_result = &(network.detect_result_group.results[i]);
			sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
			printf("%s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left,
			       det_result->box.top, det_result->box.right,
			       det_result->box.bottom, // ???????????????,????????????????????????????????????
			       det_result->prop);
		}
		printf("--------printf OK--------\n");

		printf("--------rga process start--------\n");
		// rga ??????????????????
		network.image_box.x = network.detect_result_group.results[0].box.left;
		network.image_box.y = network.detect_result_group.results[0].box.top;
		network.image_box.width = network.detect_result_group.results[0].box.right -
		                          network.detect_result_group.results[0].box.left;
		network.image_box.height = network.detect_result_group.results[0].box.bottom -
		                           network.detect_result_group.results[0].box.top;
		printf("--------configure rga box over--------\n");
		printf("-------------------box info------------------->>\n");
		printf("x=%d,\ty=%d,\t width=%d,\t height=%d\n", network.image_box.x, network.image_box.y,
		       network.image_box.width, network.image_box.height);

		ret = imdraw(dst_rga_handle, network.image_box, 0x77777777, 1);
		if (ret != IM_STATUS_SUCCESS) {
			printf("draw box failed!\n");
		}
		printf("--------box draw over--------\n");

		// ??????rga
		/*
		ret = releasebuffer_handle(src_rga_buffer_handle);
		if (ret == IM_STATUS_SUCCESS)
			printf("--------release src rga success!--------\n");

		ret = releasebuffer_handle(dst_rga_buffer_handle);
		if (ret == IM_STATUS_SUCCESS)
			printf("--------release dst success!--------\n");*/

		g_recv_data = false;
		g_process_end = true;
		// ?????????
		pthread_mutex_unlock(&g_network_lock);
	} while (g_npu_run);

	printf("--------npu_process thread exit--------\n");
	pthread_exit((void *)-1);
}

int resize(void *src_ptr, int src_width, int src_height, int src_fmt, void *dst_ptr, int dst_width,
           int dst_height, int dst_fmt, int rotation) {

	printf("start---------------->>");
	rga_info_t src, dst;
	static bool is_init = false;

	if (!is_init) {
		is_init = true;
		c_RkRgaInit();
	}
	printf("---------rga init------------\n");
	memset(&src, 0, sizeof(rga_info_t));
	src.fd = -1;
	src.virAddr = src_ptr;
	src.mmuFlag = 1;
	src.rotation = rotation;

	memset(&dst, 0, sizeof(rga_info_t));
	dst.fd = -1;
	dst.virAddr = dst_ptr;
	dst.mmuFlag = 1;
	dst.nn.nn_flag = 0;

	// ??????????????????
	int src_s = MIN(src_width, src_height);
	int src_xoffset = (src_width - src_s) / 2;
	int src_yoffset = (src_height - src_s) / 2;
	rga_set_rect(&src.rect, src_xoffset, src_yoffset, src_s, src_s, src_width, src_height, src_fmt);

	int dst_s = MIN(dst_width, dst_height);
	int dst_xoffset = (dst_width - dst_s) / 2;
	int dst_yoffset = (dst_height - dst_s) / 2;
	rga_set_rect(&dst.rect, dst_xoffset, dst_yoffset, dst_s, dst_s, dst_width, dst_height, dst_fmt);
	if (c_RkRgaBlit(&src, &dst, NULL)) {
		printf("%s: rga fail\n", __func__);
		return -1;
	}

	printf("end---------------->>");

	return 0;
}
