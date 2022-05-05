#include "./npu.h"

#include <vector>
#include "../include/rk_comm_video.h"

static network_info_t network;
static rknn_tensor_type input_type = RKNN_TENSOR_UINT8;
static rknn_tensor_format input_layout = RKNN_TENSOR_NHWC;

extern bool g_npu_run;
extern bool g_recv_data;
extern bool g_process_end;
extern pthread_mutex_t g_network_lock;
extern unsigned char *g_input_data;
extern int g_img_width;
extern int g_img_height;

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
	int ret = 0;

	rknn_context ctx = 0;

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

	// 开启NPU处理线程
	pthread_create(&g_npu_proc, NULL, npu_process, (void *)ctx);
	return 0;
}

// network deinit
void network_exit(rknn_context *ctx) {
	g_npu_run = false;

	if (g_npu_proc) {
		pthread_join(g_npu_proc, NULL);
		g_npu_proc = 0;
	}

	rknn_destroy(*ctx);

	if (network.buf)
		free(network.buf);
	if (network.input_attrs)
		free(network.input_attrs);
	if (network.output_attrs)
		free(network.output_attrs);
	if (network.inputs)
		free(network.inputs);
	if (network.outputs)
		free(network.outputs);
}

// npu process
void * npu_process(void *arg) {

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
		printf("recv data success! will process it\n");
		printf("--------allocate input memory--------\n");
		// 分配输入内存
		rknn_tensor_mem *input_mems[1];
		printf("array size=%d\n", network.input_attrs[0].size_with_stride);
		input_mems[0] = rknn_create_mem(*ctx, network.input_attrs[0].size_with_stride);
		printf("--------allocate over--------\n");

		// 将输入数据拷贝到已经分配的内存上
		int width = network.input_attrs[0].dims[2];
		int w_stride = network.input_attrs[0].w_stride;
		int h_stride = network.input_attrs[0].h_stride;
		printf("--------copy data to input memory--------\n");
		if (width == w_stride) //如果没有像素补齐
		{
			// 内存拷贝 dst src size=H*W*C
			memcpy(input_mems[0]->virt_addr, g_input_data,
			       width * network.input_attrs[0].dims[1] * network.input_attrs[0].dims[3]);
		} else // 如果进行了像素补齐
		{
			int height = network.input_attrs[0].dims[1];
			int channel = network.input_attrs[0].dims[3];
			// copy from src to dst with w_stride
			uint8_t *src_ptr = g_input_data;
			uint8_t *dst_ptr = (uint8_t *)input_mems[0]->virt_addr;
			// width-channel elements
			int src_wc_elems = width * channel;
			int dst_wc_elems = w_stride * channel;
			for (int h = 0; h < height; ++h) {
				memcpy(dst_ptr, src_ptr, src_wc_elems);
				src_ptr += src_wc_elems;
				dst_ptr += dst_wc_elems; // 图片左对齐进行拷贝,右边补齐元素0,按照行*通道进行拷贝
			}
		}
		printf("--------copy over--------\n");
		printf("--------allocate output memory--------\n");
		// 分配输出内存
		rknn_tensor_mem *output_mems[network.io_num.n_output];
		for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
			output_mems[i] = rknn_create_mem(*ctx, network.output_attrs[i].size_with_stride);
		}
		printf("--------allocate output memory over--------\n");
		// 设置输入的结构体信息
		ret = rknn_set_io_mem(*ctx, input_mems[0], &network.input_attrs[0]);
		if (ret < 0) {
			printf("rknn_set_io_mem fail! ret=%d\n", ret);
			pthread_exit((void *)-1);
		}

		// 设置输出的结构体信息
		for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
			// set output memory and attribute
			ret = rknn_set_io_mem(*ctx, output_mems[i], &network.output_attrs[i]);
			if (ret < 0) {
				printf("rknn_set_io_mem fail! ret=%d\n", ret);
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

		// 查询网络推理的输出
		printf("output origin tensors:\n");
		rknn_tensor_attr orig_output_attrs[network.io_num.n_output];
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

		// 分配输出NCHW的结构体内存
		rknn_tensor_mem *output_mems_nchw[network.io_num.n_output];
		for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
			int size = orig_output_attrs[i].size_with_stride;
			output_mems_nchw[i] = rknn_create_mem(*ctx, size);
		}

		for (uint32_t i = 0; i < network.io_num.n_output; i++) {
			int channel = orig_output_attrs[i].dims[1];
			int h = orig_output_attrs[i].n_dims > 2 ? orig_output_attrs[i].dims[2] : 1;
			int w = orig_output_attrs[i].n_dims > 3 ? orig_output_attrs[i].dims[3] : 1;
			int hw = h * w;
			int zp = network.output_attrs[i].zp;
			float scale = network.output_attrs[i].scale;
			// 拷贝输出的NCHW数据到预先分配的输出内存上  模型默认输出NC1HWC2,需要转换为NCHW
			NC1HWC2_to_NCHW((int8_t *)output_mems[i]->virt_addr,
			                (int8_t *)output_mems_nchw[i]->virt_addr, network.output_attrs[i].dims,
			                channel, hw);
		}

		int model_width = 0;
		int model_height = 0;
		if (network.input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
			printf("model is NCHW input fmt\n");
			model_width = network.input_attrs[0].dims[2];
			model_height = network.input_attrs[0].dims[3];
		} else {
			printf("model is NHWC input fmt\n");
			model_width = network.input_attrs[0].dims[1];
			model_height = network.input_attrs[0].dims[2];
		}
		// post process
		network.scale_w = (float)model_width / g_img_width;   //输入的图片的宽
		network.scale_h = (float)model_height / g_img_height; //输入图片的高

		for (int i = 0; i < network.io_num.n_output; ++i) {
			network.out_scales.push_back(network.output_attrs[i].scale);
			network.out_zps.push_back(network.output_attrs[i].zp);
		}
	
		post_process((int8_t *)output_mems_nchw[0]->virt_addr,
		             (int8_t *)output_mems_nchw[1]->virt_addr,
		             (int8_t *)output_mems_nchw[2]->virt_addr, 640, 640, network.box_conf_threshold,
		             network.nms_threshold, network.scale_w, network.scale_h, network.out_zps,
		             network.out_scales, &network.detect_result_group);

		char text[256];
		for (int i = 0; i < network.detect_result_group.count; i++) {
			detect_result_t *det_result = &(network.detect_result_group.results[i]);
			sprintf(text, "%s %.1f%%", det_result->name, det_result->prop * 100);
			printf("%s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left,
			       det_result->box.top, det_result->box.right,
			       det_result->box.bottom, // 检测的结果,以及框在原来图像上的位置
			       det_result->prop);
		}

		// rga 框的位置信息及配置信息
		network.image_box.x = network.detect_result_group.results[0].box.left;
		network.image_box.y = network.detect_result_group.results[0].box.top;
		network.image_box.width = network.detect_result_group.results[0].box.right-network.detect_result_group.results[0].box.left;
		network.image_box.height = network.detect_result_group.results[0].box.bottom-network.detect_result_group.results[0].box.top;
		im_handle_param_t * param;
		param->width = w_stride;
		param->height = h_stride;
		param->format = 65553;
		//rga 绘制框
		rga_buffer_handle_t rga_buffer_handle;
		rga_buffer_handle = importbuffer_virtualaddr((void *)g_input_data, param);
		rga_buffer_t wrap_handle;
		wrap_handle = wrapbuffer_handle(rga_buffer_handle, g_img_width, g_img_height, g_img_width, g_img_height, 1);
		ret = imdraw(wrap_handle, network.image_box, 0x77777777, 1);
		if(ret != IM_STATUS_SUCCESS)
		{
			printf("draw box failed!\n");
		}
		// 释放rga
		ret = releasebuffer_handle(rga_buffer_handle);
		if(ret == IM_STATUS_SUCCESS) printf("release rga success!\n");

		g_recv_data = false;
		g_process_end = true;
		// 释放锁
		pthread_mutex_unlock(&g_network_lock);
		
		// Destroy rknn memory
		rknn_destroy_mem(*ctx, input_mems[0]);
		for (uint32_t i = 0; i < network.io_num.n_output; ++i) {
			rknn_destroy_mem(*ctx, output_mems[i]);
			rknn_destroy_mem(*ctx, output_mems_nchw[i]);
		}
		// destroy
		rknn_destroy(*ctx);

		free(g_input_data);
		g_recv_data = false;
	} while (g_npu_run);
	printf("npu_process thread exit\n");
	pthread_exit((void *)-1);
}
