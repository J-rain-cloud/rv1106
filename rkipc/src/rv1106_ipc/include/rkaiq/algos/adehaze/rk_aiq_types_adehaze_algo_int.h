/*
 *rk_aiq_types_adehaze_algo_int.h
 *
 *  Copyright (c) 2019 Rockchip Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef _RK_AIQ_TYPE_ADEHAZE_ALGO_INT_H_
#define _RK_AIQ_TYPE_ADEHAZE_ALGO_INT_H_
#include "RkAiqCalibDbTypes.h"
#include "adehaze_head.h"
#include "adehaze/rk_aiq_types_adehaze_algo.h"

#define DEHAZE_API_MANUAL_DEFAULT_LEVEL         (50)
#define DEHAZE_API_MANUAL_DEFAULT_CFG_WT_STEP   (0.005)
#define DEHAZE_API_MANUAL_DEFAULT_CFG_AIR_STEP  (0.5)
#define DEHAZE_API_MANUAL_DEFAULT_CFG_TMAX_STEP (0.005)
#define DEHAZE_API_ENHANCE_MANUAL_DEFAULT_LEVEL (50)
#define DEHAZE_API_ENHANCE_MANUAL_DEFAULT_STEP  (0.08)

// dehaze v10
typedef enum dehaze_api_mode_s {
    DEHAZE_API_AUTO   = 0, /**< run atuo mode */
    DEHAZE_API_MANUAL = 1, /**< run manual mode*/
} dehaze_api_mode_t;

typedef struct adehaze_sw_v10_s {
} adehaze_sw_v10_t;

// dehaze v11
typedef struct mDehazeDataV11_s {
    float dc_min_th;
    float dc_max_th;
    float yhist_th;
    float yblk_th;
    float dark_th;
    float bright_min;
    float bright_max;
    float wt_max;
    float air_min;
    float air_max;
    float tmax_base;
    float tmax_off;
    float tmax_max;
    float cfg_wt;
    float cfg_air;
    float cfg_tmax;
    float dc_weitcur;
    float bf_weight;
    float range_sigma;
    float space_sigma_pre;
    float space_sigma_cur;
} mDehazeDataV11_t;

typedef struct mDehaze_setting_v11_s {
    bool en;
    bool air_lc_en;
    float stab_fnum;
    float sigma;
    float wt_sigma;
    float air_sigma;
    float tmax_sigma;
    float pre_wet;
    mDehazeDataV11_t DehazeData;
} mDehaze_setting_v11_t;

typedef struct mEnhanceDataV11_s {
    float enhance_value;
    float enhance_chroma;
} mEnhanceDataV11_t;

typedef struct mEnhance_setting_v11_s {
    bool en;
    float enhance_curve[CALIBDB_ADEHAZE_ENHANCE_CURVE_KNOTS_NUM];
    mEnhanceDataV11_t EnhanceData;
} mEnhance_setting_v11_t;

typedef struct mHistDataV11_s {
    float hist_gratio;
    float hist_th_off;
    float hist_k;
    float hist_min;
    float hist_scale;
    float cfg_gratio;
} mHistDataV11_t;

typedef struct mHist_setting_v11_s {
    bool en;
    bool hist_para_en;
    mHistDataV11_t HistData;
} mHist_setting_v11_t;

typedef struct mDehazeAttrV11_s {
    bool Enable;
    float cfg_alpha;
    mDehaze_setting_v11_t dehaze_setting;
    mEnhance_setting_v11_t enhance_setting;
    mHist_setting_v11_t hist_setting;
    double sigma_curve[5];
} mDehazeAttrV11_t;

typedef struct mDehazeAttrInfoV11_s {
    float ISO;
    float EnvLv;
    unsigned int MDehazeStrth;
    unsigned int MEnhanceStrth;
    unsigned int MEnhanceChromeStrth;
} mDehazeAttrInfoV11_t;

typedef struct adehaze_sw_v11_s {
    rk_aiq_uapi_sync_t sync;

    dehaze_api_mode_t mode;
    CalibDbV2_dehaze_v11_t stAuto;
    mDehazeAttrV11_t stManual;
    mDehazeAttrInfoV11_t Info;
} adehaze_sw_v11_t;

// dehaze v12
typedef struct mEnhance_setting_v12_s {
    bool en;
    bool color_deviate_en;
    bool enh_luma_en;
    float enhance_curve[CALIBDB_ADEHAZE_ENHANCE_CURVE_KNOTS_NUM];
    float enh_luma[CALIBDB_ADEHAZE_ENHANCE_CURVE_KNOTS_NUM];
    mEnhanceDataV11_t EnhanceData;
} mEnhance_setting_v12_t;

typedef struct mDehazeAttrV12_s {
    bool Enable;
    float cfg_alpha;
    mDehaze_setting_v11_t dehaze_setting;
    mEnhance_setting_v12_t enhance_setting;
    mHist_setting_v11_t hist_setting;
    double sigma_curve[5];
} mDehazeAttrV12_t;

typedef struct adehaze_sw_v12_s {
    rk_aiq_uapi_sync_t sync;

    dehaze_api_mode_t mode;
    CalibDbV2_dehaze_v12_t stAuto;
    mDehazeAttrV12_t stManual;
    mDehazeAttrInfoV11_t Info;
} adehaze_sw_v12_t;

#endif
