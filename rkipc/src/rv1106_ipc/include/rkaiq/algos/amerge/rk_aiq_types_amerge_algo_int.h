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
#ifndef __RK_AIQ_TYPES_AMERGE_ALGO_INT_H__
#define __RK_AIQ_TYPES_AMERGE_ALGO_INT_H__

#include "RkAiqCalibDbTypes.h"
#include "amerge_head.h"
#include "amerge_uapi_head.h"

typedef enum merge_OpMode_e {
    MERGE_OPMODE_AUTO   = 0,  // run auto merge
    MERGE_OPMODE_MANUAL = 1,  // run manual merge
} merge_OpMode_t;

typedef uapiMergeCurrCtlData_t MergeCurrCtlData_t;

// merge V10
typedef struct mMergeOECurveV10_s {
    float Smooth;
    float Offset;
} mMergeOECurveV10_t;

typedef struct mMergeMDCurveV10_s {
    float LM_smooth;
    float LM_offset;
    float MS_smooth;
    float MS_offset;
} mMergeMDCurveV10_t;

typedef struct mmergeAttrV10_s {
    mMergeOECurveV10_t OECurve;
    mMergeMDCurveV10_t MDCurve;
} mmergeAttrV10_t;

typedef struct mergeAttrV10_s {
    rk_aiq_uapi_sync_t sync;

    merge_OpMode_t opMode;
    mmergeAttrV10_t stManual;
    CalibDbV2_merge_V10_t stAuto;
    MergeCurrCtlData_t   CtlInfo;
} mergeAttrV10_t;

// merge V11
typedef struct mLongFrameModeData_s {
    mMergeOECurveV10_t OECurve;
    mMergeMDCurveV10_t MDCurve;
} mLongFrameModeData_t;

typedef struct mMergeMDCurveV11Short_s {
    float Coef;
    float ms_thd0;
    float lm_thd0;
} mMergeMDCurveV11Short_t;

typedef struct mShortFrameModeData_s {
    mMergeOECurveV10_t OECurve;
    mMergeMDCurveV11Short_t MDCurve;
} mShortFrameModeData_t;

typedef struct mMergeAttrV11_s {
    MergeBaseFrame_t BaseFrm;
    mLongFrameModeData_t LongFrmModeData;
    mShortFrameModeData_t ShortFrmModeData;
} mMergeAttrV11_t;

typedef struct mergeAttrV11_s {
    rk_aiq_uapi_sync_t sync;

    merge_OpMode_t opMode;
    mMergeAttrV11_t stManual;
    CalibDbV2_merge_V11_t stAuto;
    MergeCurrCtlData_t CtlInfo;
} mergeAttrV11_t;

// merge V12
typedef struct mMergeEachChnCurveV12_s {
    float Smooth;
    float Offset;
} mMergeEachChnCurveV12_t;

typedef struct mLongFrameModeDataV12_s {
    bool EnableEachChn;
    mMergeOECurveV10_t OECurve;
    mMergeMDCurveV10_t MDCurve;
    mMergeEachChnCurveV12_t EachChnCurve;
} mLongFrameModeDataV12_t;

typedef struct mMergeAttrV12_s {
    MergeBaseFrame_t BaseFrm;
    mLongFrameModeDataV12_t LongFrmModeData;
    mShortFrameModeData_t ShortFrmModeData;
} mMergeAttrV12_t;

typedef struct mergeAttrV12_s {
    rk_aiq_uapi_sync_t sync;

    merge_OpMode_t opMode;
    mMergeAttrV12_t stManual;
    CalibDbV2_merge_V12_t stAuto;
    MergeCurrCtlData_t CtlInfo;
} mergeAttrV12_t;

#endif
