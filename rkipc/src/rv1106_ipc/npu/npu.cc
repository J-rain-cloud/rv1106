#include "npu.h"

#include "../include/rk_comm_video.h"
#include "RgaApi.h"
#include <vector>

static network_info_t network;
static rknn_tensor_type input_type = RKNN_TENSOR_UINT8;
static rknn_tensor_format input_layout = RKNN_TENSOR_NHWC;
static rknn_context ctx = 0;
static int ret = 0;

extern bool g_npu_run;
extern bool g_recv_data;
extern bool g_process_end;
extern pthread_mutex_t g_network_lock;
extern void *g_input_data;
extern int g_img_width;
extern int g_img_height;
extern PIXEL_FORMAT_E enPixelFormat;
extern VIDEO_FORMAT_E enVideoFormat;

pthread_t g_npu_proc;

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

// network init
int network_init(char *model_path) {
	// load model
	ret = rknn_init(&ctx, model_path, 0, 0, NULL);
	if (ret < 0) {
		printf("rknn_init fail! ret=%d\n", ret);
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

	//网络输入格式
	network.input_attrs[0].type = input_type;
	network.input_attrs[0].fmt =
	    input_layout; // default fmt is NHWC, npu only support NHWC in zero copy mode

	printf("--------network init over--------\n");

	//网络的输入通道宽高
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

	//缩放尺度
	network.scale_w = (float)network.width / g_img_width;
	network.scale_h = (float)network.height / g_img_height;

	//配置rga图像缓冲区的参数
	network.param = (im_handle_param_t *)malloc(sizeof(im_handle_param_t));
	memset(network.param, 0, sizeof(im_handle_param_t));
	network.param->width = network.input_attrs[0].w_stride + network.width;
	network.param->height = network.height;
	network.param->format = enPixelFormat;
	printf("--------configure param over--------\n");

	//模型输入图片缓存
	network.buf = malloc(sizeof(unsigned char) * network.width * network.height * network.channel);
	memset(network.buf, 0,
		       sizeof(unsigned char) * (network.height * network.width * network.channel));

	// 分配输入内存
	network.input_mems[0] = rknn_create_mem(ctx, network.input_attrs[0].size_with_stride);
	if (network.input_mems[0] == NULL) {
		printf("--------allocate input memory failed--------\n");
		return -1;
	}

	// 分配输出内存
	for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
		network.output_mems[i] = rknn_create_mem(ctx, network.output_attrs[i].size_with_stride);
		if (network.output_mems[i] == NULL) {
			printf("--------allocate output memory failed--------\n");
			return -1;
		}
	}

	// 设置输入的结构体信息
	ret = rknn_set_io_mem(ctx, network.input_mems[0], &network.input_attrs[0]);
	if (ret < 0) {
		printf("input_memory: rknn_set_io_mem fail! ret=%d\n", ret);
		return -1;
	}

	// 设置输出的结构体信息
	for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
		// set output memory and attribute
		ret = rknn_set_io_mem(ctx, network.output_mems[i], &network.output_attrs[i]);
		if (ret < 0) {
			printf("output_memory: rknn_set_io_mem fail! ret=%d\n", ret);
			return -1;
		}
	}
	printf("--------set in_out put struct info over--------\n");

	// 开启NPU处理线程
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
	if (network.param)
		free(network.param);
	if (network.input_attrs)
		free(network.input_attrs);
	if (network.output_attrs)
		free(network.output_attrs);

	if (network.input_mems)
		rknn_destroy_mem(ctx, network.input_mems[0]);
	if (network.output_mems) {
		rknn_destroy_mem(ctx, network.output_mems[0]);
		rknn_destroy_mem(ctx, network.output_mems[1]);
		rknn_destroy_mem(ctx, network.output_mems[2]);
	}

	printf("--------network deinit over--------\n");
}

// npu process
void *npu_process(void *arg) {

	printf("--------%s thread start--------\n", __func__);
	rknn_context *ctx = (rknn_context *)arg;
	int ret = 0;

	do { // 没有拿到互斥锁,这堵塞等待解锁
		pthread_mutex_lock(&g_network_lock);
		//拿到锁后判断是否有数据  input_data
		if (g_recv_data == false) {
			pthread_mutex_unlock(&g_network_lock);
			continue;
		}
		//有数据则进行下一步
		printf("--------recv data success! will process it--------\n");
		// 拷贝一份原始数据
		// memcpy(network.buf, (void *)g_input_data, network.height * network.width * network.channel);

		//映射虚拟地址到RGA驱动内部
		rga_buffer_handle_t src_rga_buffer_handle;
		im_handle_param_t * param = (im_handle_param_t *)malloc(sizeof(im_handle_param_t));
		param->width = g_img_width;
		param->height = g_img_height;
		param->format = enPixelFormat;
		src_rga_buffer_handle = importbuffer_virtualaddr(g_input_data, param);
		rga_buffer_handle_t dst_rga_buffer_handle;
		dst_rga_buffer_handle = importbuffer_virtualaddr(network.buf, network.param);
		printf("-------- reflect virtual address over--------\n");

		// 转换格式为标准的 rga_buffer_t
		rga_buffer_t src_rga_handle;
		src_rga_handle = wrapbuffer_handle(src_rga_buffer_handle, g_img_width, g_img_height,
		                                   0, 0, enPixelFormat);
		rga_buffer_t dst_rga_handle;
		dst_rga_handle = wrapbuffer_handle(dst_rga_buffer_handle, network.width, network.height,
		                                   0, 0, enPixelFormat);
		printf("-------- transfer image params to rga_buffer_t over--------\n");

		//将图片resize为模型需要的尺寸640*640
		ret = imresize(src_rga_handle, dst_rga_handle, network.scale_w, network.scale_h,
		               INTER_NEAREST, 1);
		printf("-------- width=%d,\twstride=%d,\tprt=%p,\theight=%d,\thstride=%d--------\n", src_rga_handle.width,
		       src_rga_handle.wstride, src_rga_handle.vir_addr, src_rga_handle.height, src_rga_handle.hstride);
		printf("-------- width=%d,\twstride=%d,\tprt=%p,\theight=%d,\thstride=%d--------\n", dst_rga_handle.width,
		       dst_rga_handle.wstride, dst_rga_handle.vir_addr, dst_rga_handle.height, dst_rga_handle.hstride);
		if (ret != IM_STATUS_SUCCESS) {
			printf("--------resize image failed--------\n");
			pthread_exit((void *)-1);
		}
		printf("--------resize image over--------\n");

		// 将输入数据拷贝到已经分配的内存上
		int w_stride = network.input_attrs[0].w_stride;

		printf("--------copy data to input memory--------\n");
		if (w_stride == 0) //如果没有像素补齐
		{
			printf("--------0 pixel fill--------\n");
			// 内存拷贝 dst src size=H*W*C
			if (network.input_mems[0]->virt_addr) {
				printf("--------memory copy--------\n");
				memcpy(network.input_mems[0]->virt_addr, dst_rga_handle.vir_addr,
				       network.width * network.height * network.channel);
			} else
				printf("--------address error--------\n");

		} else // 如果进行了像素补齐
		{
			printf("--------pixel fill--------\n");
			// copy from src to dst with w_stride
			uint8_t *src_ptr = (uint8_t *)dst_rga_handle.vir_addr;
			uint8_t *dst_ptr = (uint8_t *)network.input_mems[0]->virt_addr;
			// width-channel elements
			int src_wc_elems = network.width * network.channel;
			int dst_wc_elems = (w_stride + network.width) * network.channel;
			for (int h = 0; h < network.height; ++h) {
				memcpy(dst_ptr, src_ptr, src_wc_elems);
				src_ptr += src_wc_elems;
				dst_ptr += dst_wc_elems; // 图片左对齐进行拷贝,右边补齐元素0,按照行*通道进行拷贝
			}
		}
		printf("--------copy over--------\n");

		// Run
		printf("--------model start run--------\n");
		ret = rknn_run(*ctx, NULL);
		if (ret < 0) {
			printf("rknn run error %d\n", ret);
			pthread_exit((void *)-1);
		}

		// 查询网络推理的原始输出
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
		// 分配输出NCHW的结构体内存
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
			// 拷贝输出的NCHW数据到预先分配的输出内存上  模型默认输出NC1HWC2,需要转换为NCHW
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
			       det_result->box.bottom, // 检测的结果,以及框在原来图像上的位置
			       det_result->prop);
		}
		printf("--------printf OK--------\n");

		printf("--------rga process start--------\n");
		// rga 框的位置信息
		network.image_box.x = network.detect_result_group.results[0].box.left;
		network.image_box.y = network.detect_result_group.results[0].box.top;
		network.image_box.width = network.detect_result_group.results[0].box.right -
		                          network.detect_result_group.results[0].box.left;
		network.image_box.height = network.detect_result_group.results[0].box.bottom -
		                           network.detect_result_group.results[0].box.top;
		printf("--------configure rga box over--------\n");
		printf("-------------------box info------------------->>");
		printf("x=%d,\ty=%d,\t width=%d,\t height=%d\n", network.image_box.x, network.image_box.y,
		       network.image_box.width, network.image_box.height);

		ret = imdraw(dst_rga_handle, network.image_box, 0x77777777, 1);
		if (ret != IM_STATUS_SUCCESS) {
			printf("draw box failed!\n");
		}
		printf("--------box draw over--------\n");

		// 释放rga
		ret = releasebuffer_handle(src_rga_buffer_handle);
		if (ret == IM_STATUS_SUCCESS)
			printf("--------release src rga success!--------\n");

		ret = releasebuffer_handle(dst_rga_buffer_handle);
		if (ret == IM_STATUS_SUCCESS)
			printf("--------release dst success!--------\n");

		g_recv_data = false;
		g_process_end = true;
		// 释放锁
		pthread_mutex_unlock(&g_network_lock);
		free(param);
		g_recv_data = false;
	} while (g_npu_run);

	printf("--------npu_process thread exit--------\n");
	pthread_exit((void *)-1);
}

int resize(void *src_ptr, int src_width, int src_height, int src_fmt, void *dst_ptr, int dst_width,
           int dst_height, int dst_fmt, int rotation) {
	rga_info_t src, dst;
	static bool is_init = false;

	if (!is_init) {
		is_init = true;
		c_RkRgaInit();
	}

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

	// 裁切中间区域
	int src_s = std::min(src_width, src_height);
	int src_xoffset = (src_width - src_s) / 2;
	int src_yoffset = (src_height - src_s) / 2;
	rga_set_rect(&src.rect, src_xoffset, src_yoffset, src_s, src_s, src_width, src_height, src_fmt);

	int dst_s = std::min(dst_width, dst_height);
	int dst_xoffset = (dst_width - dst_s) / 2;
	int dst_yoffset = (dst_height - dst_s) / 2;
	rga_set_rect(&dst.rect, dst_xoffset, dst_yoffset, dst_s, dst_s, dst_width, dst_height, dst_fmt);
	if (c_RkRgaBlit(&src, &dst, NULL)) {
		printf("%s: rga fail\n", __func__);
		return -1;
	}

	return 0;
}
