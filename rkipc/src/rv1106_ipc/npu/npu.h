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
    int width;                      // 输入图片的宽度
    int height;                     // 输入图片的高度 
    int channel;                    // 输入图片的通道数量
    void *buf;                      // 输入图片数据 
    rknn_input_output_num io_num;   // 输入输出端口数量 
    rknn_tensor_attr *input_attrs;  // 网络的输入属性 
    rknn_tensor_attr *output_attrs; // 网络的输出属性 
    rknn_input *inputs;             // 网络输入数据 
    rknn_output *outputs;           // 网络输出数据 
    rknn_custom_string custom_string; //custom string by user
    
    std::vector<float> out_scales;
    std::vector<uint32_t> out_zps;

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
void network_exit(rknn_context *ctx);

void * npu_process(void *data);

#ifdef __cplusplus
}
#endif

#endif //__NPU_H__