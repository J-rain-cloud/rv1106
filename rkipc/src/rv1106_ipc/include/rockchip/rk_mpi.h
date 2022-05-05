/*
 * Copyright 2015 Rockchip Electronics Co. LTD
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
 */

#ifndef __RK_MPI_H__
#define __RK_MPI_H__

/**
 * @addtogroup rk_mpi
 * @brief Rockchip Media Process Interface
 * @details Media Process Platform(MPP) provides application programming
 *          interface for the application layer, by which applications can
 *          call hardware encode and decode. Current MPP fully supports
 *          chipset RK3288/RK3228/RK3229/RK3399/RK3328/RV1108. Old chipset
 *          like RK29xx/RK30xx/RK31XX/RK3368 is partly supported due to lack
 *          of some hardware register generation module.
 */

#include "rk_mpi_cmd.h"
#include "mpp_task.h"

/**
 * @ingroup rk_mpi
 * @brief MPP main work function set
 * @details all api function are seperated into two sets: data io api set
 *          and control api set
 *
 * (1). the data api set is for data input/output flow including:
 *
 * (1.1) simple data api set:
 *
 * decode   : both send video stream packet to decoder and get video frame from
 *            decoder at the same time.
 *
 * encode   : both send video frame to encoder and get encoded video stream from
 *            encoder at the same time.
 *
 * decode_put_packet: send video stream packet to decoder only, async interface
 *
 * decode_get_frame : get video frame from decoder only, async interface
 *
 * encode_put_frame : send video frame to encoder only, async interface
 *
 * encode_get_packet: get encoded video packet from encoder only, async interface
 *
 * (1.2) advanced task api set:
 *
 * poll     : poll port for dequeue
 *
 * dequeue  : pop a task from mpp task queue
 *
 * enqueue  : push a task to mpp task queue
 *
 * (2). the control api set is for mpp context control including:
 *
 * control  : similiar to ioctl in kernel driver, setup or get mpp internal parameter
 *
 * reset    : clear all data in mpp context, discard all packet and frame,
 *            reset all components to initialized status
 */
typedef struct MppApi_t {
    /**
     * @brief size of struct MppApi
     */
    RK_U32  size;
    /**
     * @brief mpp api version, generated by Git
     */
    RK_U32  version;

    // simple data flow interface
    /**
     * @brief both send video stream packet to decoder and get video frame from
     *        decoder at the same time
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[in] packet The input video stream, its usage can refer mpp_packet.h.
     * @param[out] frame The output picture, its usage can refer mpp_frame.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*decode)(MppCtx ctx, MppPacket packet, MppFrame *frame);
    /**
     * @brief send video stream packet to decoder only, async interface
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[in] packet The input video stream, its usage can refer mpp_packet.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*decode_put_packet)(MppCtx ctx, MppPacket packet);
    /**
     * @brief get video frame from decoder only, async interface
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[out] frame The output picture, its usage can refer mpp_frame.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*decode_get_frame)(MppCtx ctx, MppFrame *frame);
    /**
     * @brief both send video frame to encoder and get encoded video stream from
     *        encoder at the same time
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[in] frame The input video data, its usage can refer mpp_frame.h.
     * @param[out] packet The output compressed data, its usage can refer mpp_packet.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*encode)(MppCtx ctx, MppFrame frame, MppPacket *packet);
    /**
     * @brief send video frame to encoder only, async interface
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[in] frame The input video data, its usage can refer mpp_frame.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*encode_put_frame)(MppCtx ctx, MppFrame frame);
    /**
     * @brief get encoded video packet from encoder only, async interface
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[out] packet The output compressed data, its usage can refer mpp_packet.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*encode_get_packet)(MppCtx ctx, MppPacket *packet);

    /**
     * @brief ISP interface, will be supported in the future.
     */
    MPP_RET (*isp)(MppCtx ctx, MppFrame dst, MppFrame src);
    /**
     * @brief ISP interface, will be supported in the future.
     */
    MPP_RET (*isp_put_frame)(MppCtx ctx, MppFrame frame);
    /**
     * @brief ISP interface, will be supported in the future.
     */
    MPP_RET (*isp_get_frame)(MppCtx ctx, MppFrame *frame);

    // advance data flow interface
    /**
     * @brief poll port for dequeue
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[in] type input port or output port which are both for data transaction
     * @param[in] timeout mpp poll type, its usage can refer mpp_task.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*poll)(MppCtx ctx, MppPortType type, MppPollType timeout);
    /**
     * @brief dequeue MppTask, pop a task from mpp task queue
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[in] type input port or output port which are both for data transaction
     * @param[out] task MppTask popped from mpp task queue, its usage can refer mpp_task.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*dequeue)(MppCtx ctx, MppPortType type, MppTask *task);
    /**
     * @brief enqueue MppTask, push a task to mpp task queue
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[in] type input port or output port which are both for data transaction
     * @param[in] task MppTask which is sent to mpp for process, its usage can refer mpp_task.h.
     * @return 0 and positive for success, negative for failure. The return
     *         value is an error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*enqueue)(MppCtx ctx, MppPortType type, MppTask task);

    // control interface
    /**
     * @brief discard all packet and frame, reset all component,
     *        for both decoder and encoder
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @return 0 for success, others for failure. The return value is an
     *         error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*reset)(MppCtx ctx);
    /**
     * @brief control function for mpp property setting
     * @param[in] ctx The context of mpp, created by mpp_create() and initiated
     *                by mpp_init().
     * @param[in] cmd The mpi command, its definition can refer rk_mpi_cmd.h.
     * @param[in,out] param The mpi command parameter
     * @return 0 for success, others for failure. The return value is an
     *         error code. For details, please refer mpp_err.h.
     */
    MPP_RET (*control)(MppCtx ctx, MpiCmd cmd, MppParam param);


    MPP_RET (*encode_release_packet)(MppCtx ctx, MppPacket *packet);

    /**
     * @brief The reserved segment, may be used in the future
     */
    RK_U32 reserv[16];
} MppApi;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup rk_mpi
 * @brief Create empty context structure and mpi function pointers.
 *        Use functions in MppApi to access mpp services.
 * @param[in,out] ctx pointer of the mpp context, refer to MpiImpl_t.
 * @param[in,out] mpi pointer of mpi function, refer to MppApi.
 * @return 0 for success, others for failure. The return value is an
 *         error code. For details, please refer mpp_err.h.
 * @note This interface creates base flow context, all function calls
 *       are based on it.
 */
MPP_RET mpp_create(MppCtx *ctx, MppApi **mpi);
/**
 * @ingroup rk_mpi
 * @brief Call after mpp_create to setup mpp type and video format.
 *        This function will call internal context init function.
 * @param[in] ctx The context of mpp, created by mpp_create().
 * @param[in] type specify decoder or encoder, refer to MppCtxType.
 * @param[in] coding specify video compression coding, refer to MppCodingType.
 * @return 0 for success, others for failure. The return value is an
 *         error code. For details, please refer mpp_err.h.
 */
MPP_RET mpp_init(MppCtx ctx, MppCtxType type, MppCodingType coding);
/**
 * @ingroup rk_mpi
 * @brief Call after mpp_create to setup mpp type and video format.
 *        This function will call internal context init function.
 * @param[in] ctx The context of mpp, created by mpp_create().
 * @param[in] type specify decoder or encoder, refer to MppCtxType.
 * @param[in] coding specify video compression coding, refer to MppCodingType.
 * @param[in] chn specify video channel
 * @return 0 for success, others for failure. The return value is an
 *         error code. For details, please refer mpp_err.h.
 */
MPP_RET mpp_init_ext(MppCtx ctx, vcodec_attr *attr);
/**
 * @ingroup rk_mpi
 * @brief Destroy mpp context and free both context and mpi structure,
 *        it matches with mpp_init().
 * @param[in] ctx The context of mpp, created by mpp_create().
 * @return 0 for success, others for failure. The return value is an
 *         error code. For details, please refer mpp_err.h.
 */
MPP_RET mpp_destroy(MppCtx ctx);
/**
 * @ingroup rk_mpi
 * @brief judge given format is supported or not by MPP.
 * @param[in] type specify decoder or encoder, refer to MppCtxType.
 * @param[in] coding specify video compression coding, refer to MppCodingType.
 * @return 0 for support, -1 for unsupported.
 */
MPP_RET mpp_check_support_format(MppCtxType type, MppCodingType coding);
/**
 * @ingroup rk_mpi
 * @brief List all formats supported by MPP
 * @param NULL no need to input parameter
 * @return No return value. This function just prints format information supported
 *         by MPP on standard output.
 */
void    mpp_show_support_format(void);
void    mpp_show_color_format(void);

#ifdef __cplusplus
}
#endif

#endif /*__RK_MPI_H__*/