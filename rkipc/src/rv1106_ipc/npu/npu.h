#ifndef __NPU_H__
#define __NPU_H__

#ifdef __cplusplus

#include <vector>
extern "C" {
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


#include "rknn_api.h"
#include "im2d.h"
#include "im2d_type.h"

#include "postprocess.h"

#define PERF_WITH_POST 1
#define _BASETSD_H

typedef struct _network{
    int width=0;                                                  // 模型需要的宽度
    int height=0;                                                 // 模型需要的高度 
    int channel=0;                                                // 模型需要的通道数量

    void *buf;                                                  // 输入图片数据 

    rknn_input_output_num io_num;                               // 输入输出端口数量 
    rknn_tensor_attr *input_attrs;                              // 网络的输入属性 
    rknn_tensor_attr *output_attrs;                             // 网络的输出属性 

    rknn_tensor_mem *input_mems[1] = {NULL};                    //输入缓存
    rknn_tensor_mem *output_mems[3] = {NULL};                   //输出缓存
    im_handle_param_t param;                                   // rga待导入图像缓冲区的参数

    rknn_custom_string custom_string;                           //自定义字符串
    
    std::vector<float> out_scales;                              //缩放尺度  quantifize
    std::vector<uint32_t> out_zps;                              //零点      quantifize

    /* 目标检测 */
    float scale_w;
    float scale_h;
    const float nms_threshold = NMS_THRESH;
    const float box_conf_threshold = BOX_THRESH;
    detect_result_group_t detect_result_group;

    /*rga处理*/
    im_rect image_box;  //rga 绘制框
}network_info_t;


int network_init(char * model_path);
void network_exit();
void recv_frame(void *ptr, int width, int height, int format, int fd);
int resize(void *src_ptr, int src_width, int src_height, int src_fmt, void *dst_ptr, int dst_width,
           int dst_height, int dst_fmt, int rotation);

void * npu_process(void *data);

int resize(void *src_ptr, int src_width, int src_height, int src_fmt, void *dst_ptr, int dst_width,
           int dst_height, int dst_fmt, int rotation);

#ifdef __cplusplus
}
#endif

#endif //__NPU_H__