/*
 *rk_aiq_types_agamma_algo_int.h
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

#ifndef _RK_AIQ_TYPE_AGAMMA_ALGO_INT_H_
#define _RK_AIQ_TYPE_AGAMMA_ALGO_INT_H_
#include "agamma/rk_aiq_types_agamma_algo.h"
#include "RkAiqCalibDbTypes.h"
#include "agamma_head.h"

// gamma v10
typedef enum rk_aiq_gamma_op_mode_s {
    RK_AIQ_GAMMA_MODE_AUTO   = 0, /**< run Auto gamma */
    RK_AIQ_GAMMA_MODE_MANUAL = 1, /**< run manual gamma */
} rk_aiq_gamma_op_mode_t;

typedef struct AgammaApiManualV10_s {
    bool Gamma_en;
    GammaType_t Gamma_out_segnum;
    uint16_t Gamma_out_offset;
    uint16_t Gamma_curve[CALIBDB_AGAMMA_KNOTS_NUM_V10];
} AgammaApiManualV10_t;

typedef struct rk_aiq_gamma_v10_attr_s {
    rk_aiq_uapi_sync_t sync;

    rk_aiq_gamma_op_mode_t mode;
    AgammaApiManualV10_t stManual;
    CalibDbV2_gamma_V10_t stAuto;
} rk_aiq_gamma_v10_attr_t;

// gamma v11
typedef struct AgammaApiManualV11_s {
    bool Gamma_en;
    uint16_t Gamma_out_offset;
    uint16_t Gamma_curve[CALIBDB_AGAMMA_KNOTS_NUM_V11];
} AgammaApiManualV11_t;

typedef struct rk_aiq_gamma_v11_attr_s {
    rk_aiq_uapi_sync_t sync;

    rk_aiq_gamma_op_mode_t mode;
    AgammaApiManualV11_t stManual;
    CalibDbV2_gamma_V11_t stAuto;
} rk_aiq_gamma_v11_attr_t;

#endif
