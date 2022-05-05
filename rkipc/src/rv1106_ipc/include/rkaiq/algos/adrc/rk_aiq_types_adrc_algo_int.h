/******************************************************************************
 *
 * Copyright 2019, Fuzhou Rockchip Electronics Co.Ltd . All rights reserved.
 * No part of this work may be reproduced, modified, distributed, transmitted,
 * transcribed, or translated into any language or computer format, in any form
 * or by any means without written permission of:
 * Fuzhou Rockchip Electronics Co.Ltd .
 *
 *
 *****************************************************************************/
#ifndef __RK_AIQ_TYPES_ADRC_ALGO_INT_H__
#define __RK_AIQ_TYPES_ADRC_ALGO_INT_H__

#include "RkAiqCalibDbTypes.h"
#include "adrc_head.h"
#include "adrc_uapi_head.h"
#include "rk_aiq_types_adrc_stat_v200.h"

typedef enum drc_OpMode_s {
    DRC_OPMODE_AUTO   = 0,  // run auto drc
    DRC_OPMODE_MANUAL = 1,  // run manual drc
} drc_OpMode_t;

// drc attr V10
typedef struct mLocalDataV10_s {
    float         LocalWeit;
    float         GlobalContrast;
    float         LoLitContrast;
} mLocalDataV10_t;

typedef struct mDrcGain_t {
    float DrcGain;
    float Alpha;
    float Clip;
} mDrcGain_t;

typedef struct mDrcHiLit_s {
    float Strength;
} mDrcHiLit_t;

typedef struct mDrcLocalV10_s {
    mLocalDataV10_t LocalTMOData;
    float curPixWeit;
    float preFrameWeit;
    float Range_force_sgm;
    float Range_sgm_cur;
    float Range_sgm_pre;
    int Space_sgm_cur;
    int Space_sgm_pre;
} mDrcLocalV10_t;

typedef struct mDrcCompress_s {
    CompressMode_t Mode;
    uint16_t Manual_curve[ADRC_Y_NUM];
} mDrcCompress_t;

typedef struct mdrcAttr_V10_s {
    bool Enable;
    mDrcGain_t DrcGain;
    mDrcHiLit_t HiLight;
    mDrcLocalV10_t LocalTMOSetting;
    mDrcCompress_t CompressSetting;
    int Scale_y[ADRC_Y_NUM];
    float Edge_Weit;
    bool  OutPutLongFrame;
    int IIR_frame;
} mdrcAttr_V10_t;

typedef struct DrcInfoV10_s {
    DrcInfo_t CtrlInfo;
    mdrcAttr_V10_t ValidParams;
} DrcInfoV10_t;

typedef struct drcAttrV10_s {
    rk_aiq_uapi_sync_t sync;

    drc_OpMode_t opMode;
    CalibDbV2_drc_V10_t stAuto;
    mdrcAttr_V10_t stManual;
    DrcInfoV10_t Info;
} drcAttrV10_t;

// drc attr V11
typedef struct mLocalDataV11_s {
    float         LocalWeit;
    int           LocalAutoEnable;
    float         LocalAutoWeit;
    float         GlobalContrast;
    float         LoLitContrast;
} mLocalDataV11_t;

typedef struct mDrcLocalV11_s {
    mLocalDataV11_t LocalData;
    float curPixWeit;
    float preFrameWeit;
    float Range_force_sgm;
    float Range_sgm_cur;
    float Range_sgm_pre;
    int Space_sgm_cur;
    int Space_sgm_pre;
} mDrcLocalV11_t;

typedef struct mdrcAttr_V11_s {
    bool Enable;
    mDrcGain_t DrcGain;
    mDrcHiLit_t HiLight;
    mDrcLocalV11_t LocalSetting;
    mDrcCompress_t CompressSetting;
    int Scale_y[ADRC_Y_NUM];
    float Edge_Weit;
    bool  OutPutLongFrame;
    int IIR_frame;
} mdrcAttr_V11_t;

typedef struct DrcInfoV11_s {
    DrcInfo_t CtrlInfo;
    mdrcAttr_V11_t ValidParams;
} DrcInfoV11_t;

typedef struct drcAttrV11_s {
    rk_aiq_uapi_sync_t sync;

    drc_OpMode_t opMode;
    CalibDbV2_drc_V11_t stAuto;
    mdrcAttr_V11_t stManual;
    DrcInfoV11_t Info;
} drcAttrV11_t;

// drc attr V11
typedef struct mHighLightDataV12_s {
    float Strength;
    float gas_t;
} mHighLightDataV12_t;

typedef struct mHighLightV12_s {
    mHighLightDataV12_t HiLightData;
    int gas_l0;
    int gas_l1;
    int gas_l2;
    int gas_l3;
} mHighLightV12_t;

typedef struct mMotionData_s {
    float MotionStr;
} mMotionData_t;

typedef struct mDrcLocalV12_s {
    mLocalDataV11_t LocalData;
    mMotionData_t MotionData;
    float curPixWeit;
    float preFrameWeit;
    float Range_force_sgm;
    float Range_sgm_cur;
    float Range_sgm_pre;
    int Space_sgm_cur;
    int Space_sgm_pre;
} mDrcLocalV12_t;

typedef struct mdrcAttr_V12_s {
    bool Enable;
    mDrcGain_t DrcGain;
    mHighLightV12_t HiLight;
    mDrcLocalV12_t LocalSetting;
    mDrcCompress_t CompressSetting;
    int Scale_y[ADRC_Y_NUM];
    float Edge_Weit;
    bool OutPutLongFrame;
    int IIR_frame;
} mdrcAttr_V12_t;

typedef struct DrcInfoV12_s {
    DrcInfo_t CtrlInfo;
    mdrcAttr_V12_t ValidParams;
} DrcInfoV12_t;

typedef struct drcAttrV12_s {
    rk_aiq_uapi_sync_t sync;

    drc_OpMode_t opMode;
    CalibDbV2_drc_V12_t stAuto;
    mdrcAttr_V12_t stManual;
    DrcInfoV12_t Info;
} drcAttrV12_t;

#endif
