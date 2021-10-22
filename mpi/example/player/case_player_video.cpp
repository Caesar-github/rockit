/*
 * Copyright (c) 2021 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "case_player_video.h"
#include "RTMediaBuffer.h"
#include "RTMediaData.h"
#include "rk_common.h"
#include "rk_mpi_vo.h"
#include "rt_mem.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#define VOP_LAYER_CLUSTER_0 0
#define VOP_LAYER_CLUSTER_1 1

static RK_S32 RKCASE_VO_GetPictureData(VO_FRAME_INFO_S *pstFrameInfo,
                                      RTMediaBuffer *buffer) {
    RK_U32 u32Size;

    RK_CHECK_POINTER(pstFrameInfo, RK_FAILURE);
    RK_CHECK_POINTER(buffer, RK_FAILURE);

    u32Size = buffer->getLength();
    memcpy(pstFrameInfo->pData, buffer->getData(), u32Size);

    return RK_SUCCESS;
}

static RK_S32 RKCASE_VO_CreateGFXData(RK_U32 u32Width, RK_U32 u32Height,
                                  RK_U32 foramt, RK_U32 value,
                                  RK_VOID **pMblk,
                                  RTMediaBuffer *buffer) {
    VO_FRAME_INFO_S stFrameInfo;
    RK_U32 u32BuffSize;

    RK_CHECK_POINTER(pMblk, RK_FAILURE);
    RK_CHECK_POINTER(buffer, RK_FAILURE);

    u32BuffSize =
       RK_MPI_VO_CreateGraphicsFrameBuffer(u32Width, u32Height, foramt, pMblk);
    if (u32BuffSize == 0) {
        RK_LOGD("can not create gfx buffer");
        return RK_FAILURE;
    }

    RK_MPI_VO_GetFrameInfo(*pMblk, &stFrameInfo);
    RKCASE_VO_GetPictureData(&stFrameInfo, buffer);

    return RK_SUCCESS;
}

static RK_S32
RKCASE_VO_StartLayer(VO_LAYER voLayer,
                const VO_VIDEO_LAYER_ATTR_S *pstLayerAttr) {
    RK_S32 s32Ret = RK_SUCCESS;

    RK_CHECK_POINTER(pstLayerAttr, RK_FAILURE);

    s32Ret = RK_MPI_VO_SetLayerSpliceMode(voLayer, VO_SPLICE_MODE_RGA);
    if (s32Ret) {
        RK_LOGD("RK_MPI_VO_SetLayerSpliceMode failed");
        return s32Ret;
    }

    s32Ret = RK_MPI_VO_SetLayerAttr(voLayer, pstLayerAttr);
    if (s32Ret) {
        RK_LOGD("Set layer attribute failed");
        return s32Ret;
    }

    s32Ret = RK_MPI_VO_EnableLayer(voLayer);
    if (s32Ret) {
        RK_LOGD("Enable layer failed");
        return s32Ret;
    }

    return s32Ret;
}

static RK_S32 RKCASE_VO_StartDev(VO_DEV voDev, VO_PUB_ATTR_S *pstPubAttr) {
    RK_S32 s32Ret = RK_SUCCESS;

    RK_CHECK_POINTER(pstPubAttr, RK_FAILURE);

    s32Ret = RK_MPI_VO_SetPubAttr(voDev, pstPubAttr);
    if (s32Ret) {
        RK_LOGD("Set public attribute failed");
        return s32Ret;
    }

    s32Ret = RK_MPI_VO_Enable(voDev);
    if (s32Ret) {
        RK_LOGD("VO enable failed");
        return s32Ret;
    }

    return s32Ret;
}

static RK_S32 RKCASE_VO_StopLayer(VO_LAYER voLayer) {
    return RK_MPI_VO_DisableLayer(voLayer);
}

static RK_S32 RKCASE_VO_StopDev(VO_DEV voDev) {
    return RK_MPI_VO_Disable(voDev);
}

static PIXEL_FORMAT_E RKCASE_FmtToRtfmt(RK_PLAYER_VO_FORMAT_E format) {
    PIXEL_FORMAT_E rtfmt;

    switch (format) {
    case VO_FORMAT_ARGB8888:
        rtfmt = RK_FMT_BGRA8888;
        break;
    case VO_FORMAT_ABGR8888:
        rtfmt = RK_FMT_RGBA8888;
        break;
    case VO_FORMAT_RGB888:
        rtfmt = RK_FMT_BGR888;
        break;
    case VO_FORMAT_BGR888:
        rtfmt = RK_FMT_RGB888;
        break;
    case VO_FORMAT_ARGB1555:
        rtfmt = RK_FMT_BGRA5551;
        break;
    case VO_FORMAT_ABGR1555:
        rtfmt = RK_FMT_RGBA5551;
        break;
    case VO_FORMAT_NV12:
        rtfmt = RK_FMT_YUV420SP;
        break;
    case VO_FORMAT_NV21:
        rtfmt = RK_FMT_YUV420SP_VU;
        break;
    default:
        RK_LOGW("invalid format: %d", format);
        rtfmt = RK_FMT_BUTT;
    }

    return rtfmt;
}

static RK_S32 RKCASE_VO_SetRtSyncInfo(VO_SYNC_INFO_S *pstSyncInfo,
                                      VIDEO_FRAMEINFO_S *pstFrmInfo) {
    RK_CHECK_POINTER(pstSyncInfo, RK_FAILURE);
    RK_CHECK_POINTER(pstFrmInfo, RK_FAILURE);

    pstSyncInfo->bIdv = pstFrmInfo->stSyncInfo.bIdv;
    pstSyncInfo->bIhs = pstFrmInfo->stSyncInfo.bIhs;
    pstSyncInfo->bIvs = pstFrmInfo->stSyncInfo.bIvs;
    pstSyncInfo->bSynm = pstFrmInfo->stSyncInfo.bSynm;
    pstSyncInfo->bIop = pstFrmInfo->stSyncInfo.bIop;
    pstSyncInfo->u16FrameRate = (pstFrmInfo->stSyncInfo.u16FrameRate > 0)
                                    ? pstFrmInfo->stSyncInfo.u16FrameRate
                                    : 60;
    pstSyncInfo->u16PixClock = (pstFrmInfo->stSyncInfo.u16PixClock > 0)
                                  ? pstFrmInfo->stSyncInfo.u16PixClock
                                  : 65000;
    pstSyncInfo->u16Hact = (pstFrmInfo->stSyncInfo.u16Hact > 0)
                              ? pstFrmInfo->stSyncInfo.u16Hact
                              : 1200;
    pstSyncInfo->u16Hbb =
        (pstFrmInfo->stSyncInfo.u16Hbb > 0) ? pstFrmInfo->stSyncInfo.u16Hbb : 24;
    pstSyncInfo->u16Hfb =
        (pstFrmInfo->stSyncInfo.u16Hfb > 0) ? pstFrmInfo->stSyncInfo.u16Hfb : 240;
    pstSyncInfo->u16Hpw =
        (pstFrmInfo->stSyncInfo.u16Hpw > 0) ? pstFrmInfo->stSyncInfo.u16Hpw : 136;
    pstSyncInfo->u16Hmid =
        (pstFrmInfo->stSyncInfo.u16Hmid > 0) ? pstFrmInfo->stSyncInfo.u16Hmid : 0;
    pstSyncInfo->u16Vact = (pstFrmInfo->stSyncInfo.u16Vact > 0)
                              ? pstFrmInfo->stSyncInfo.u16Vact
                              : 1200;
    pstSyncInfo->u16Vbb =
        (pstFrmInfo->stSyncInfo.u16Vbb > 0) ? pstFrmInfo->stSyncInfo.u16Vbb : 200;
    pstSyncInfo->u16Vfb =
        (pstFrmInfo->stSyncInfo.u16Vfb > 0) ? pstFrmInfo->stSyncInfo.u16Vfb : 194;
    pstSyncInfo->u16Vpw =
        (pstFrmInfo->stSyncInfo.u16Vpw > 0) ? pstFrmInfo->stSyncInfo.u16Vpw : 6;

    return 0;
}

static RK_S32 RKCASE_VO_SetDispRect(VO_VIDEO_LAYER_ATTR_S *pstLayerAttr,
                                    VIDEO_FRAMEINFO_S *pstFrmInfo) {
    RK_CHECK_POINTER(pstLayerAttr, RK_FAILURE);
    RK_CHECK_POINTER(pstFrmInfo, RK_FAILURE);

    if (0 < pstFrmInfo->u32FrmInfoS32x)
        pstLayerAttr->stDispRect.s32X = pstFrmInfo->u32FrmInfoS32x;
    else
        RK_LOGD("positive x less than zero, use default value");

    if (0 < pstFrmInfo->u32FrmInfoS32y)
        pstLayerAttr->stDispRect.s32Y = pstFrmInfo->u32FrmInfoS32y;
    else
        RK_LOGD("positive y less than zero, use default value");

    if (0 < pstFrmInfo->u32DispWidth &&
        pstFrmInfo->u32DispWidth < pstFrmInfo->u32ImgWidth)
        pstLayerAttr->stDispRect.u32Width = pstFrmInfo->u32DispWidth;
    else
        RK_LOGD("DispWidth use default value");

    if (0 < pstFrmInfo->u32DispHeight &&
        pstFrmInfo->u32DispHeight < pstFrmInfo->u32ImgHeight)
        pstLayerAttr->stDispRect.u32Height = pstFrmInfo->u32DispHeight;
    else
        RK_LOGD("DispHeight use default value");

    if (0 < pstFrmInfo->u32ImgWidth)
        pstLayerAttr->stImageSize.u32Width = pstFrmInfo->u32ImgWidth;
    else
        RK_LOGD("ImgWidth, use default value");

    if (0 < pstFrmInfo->u32ImgHeight)
        pstLayerAttr->stImageSize.u32Height = pstFrmInfo->u32ImgHeight;
    else
        RK_LOGD("ImgHeight use default value");

    return 0;
}

static RK_S32 RKCASE_VO_SetDispRect_Default(VO_VIDEO_LAYER_ATTR_S *pstLayerAttr,
                                              VO_DEV voDev) {
    int ret;
    VO_PUB_ATTR_S pstAttr;
    memset(&pstAttr, 0, sizeof(VO_SINK_CAPABILITY_S));

    RK_CHECK_POINTER(pstLayerAttr, RK_FAILURE);
    ret = RK_MPI_VO_GetPubAttr(voDev, &pstAttr);

    if (ret) {
        RK_LOGD("GetSinkCapability failed");
        return ret;
    }

    pstLayerAttr->stDispRect.u32Width = pstAttr.stSyncInfo.u16Hact;
    pstLayerAttr->stDispRect.u32Height = pstAttr.stSyncInfo.u16Vact;
    pstLayerAttr->stImageSize.u32Width = pstAttr.stSyncInfo.u16Hact;
    pstLayerAttr->stImageSize.u32Height = pstAttr.stSyncInfo.u16Vact;

    return ret;
}

static RK_S32 RKCASE_VO_StartChnn(VO_LAYER voLayer, VO_VIDEO_LAYER_ATTR_S *pstLayerAttr,
                                  VIDEO_FRAMEINFO_S *pstFrmInfo) {
    VO_CHN_ATTR_S stChnAttr;
    VO_CHN_PARAM_S stChnParam;
    VO_BORDER_S stBorder;
    int ret;

    stChnAttr.stRect.s32X = pstLayerAttr->stDispRect.s32X;
    stChnAttr.stRect.s32Y = pstLayerAttr->stDispRect.s32Y;
    stChnAttr.stRect.u32Width = pstLayerAttr->stDispRect.u32Width;
    stChnAttr.stRect.u32Height = pstLayerAttr->stDispRect.u32Height;
    // set priority
    stChnAttr.u32Priority = 1;

    ret = RK_MPI_VO_SetChnAttr(voLayer, pstFrmInfo->u32ChnnNum, &stChnAttr);
    if (ret) {
        RK_LOGD("RK_MPI_VO_SetChnAttr failed(%d)", ret);
        return RK_FALSE;
    }

    // set video aspect ratio
    if (pstFrmInfo->u32EnMode == CHNN_ASPECT_RATIO_MANUAL) {
        stChnParam.stAspectRatio.enMode = ASPECT_RATIO_MANUAL;
        stChnParam.stAspectRatio.stVideoRect.s32X = pstLayerAttr->stDispRect.s32X;
        stChnParam.stAspectRatio.stVideoRect.s32Y = pstLayerAttr->stDispRect.s32Y;
        stChnParam.stAspectRatio.stVideoRect.u32Width = pstLayerAttr->stDispRect.u32Width/2;
        stChnParam.stAspectRatio.stVideoRect.u32Height = pstLayerAttr->stDispRect.u32Height/2;
        RK_MPI_VO_SetChnParam(voLayer, pstFrmInfo->u32ChnnNum, &stChnParam);
    } else if (pstFrmInfo->u32EnMode == CHNN_ASPECT_RATIO_AUTO) {
        stChnParam.stAspectRatio.enMode = ASPECT_RATIO_AUTO;
        stChnParam.stAspectRatio.stVideoRect.s32X = pstLayerAttr->stDispRect.s32X;
        stChnParam.stAspectRatio.stVideoRect.s32Y = pstLayerAttr->stDispRect.s32Y;
        stChnParam.stAspectRatio.stVideoRect.u32Width = pstLayerAttr->stDispRect.u32Width;
        stChnParam.stAspectRatio.stVideoRect.u32Height = pstLayerAttr->stDispRect.u32Height;
        RK_MPI_VO_SetChnParam(voLayer, pstFrmInfo->u32ChnnNum, &stChnParam);
    }

    stBorder.stBorder.u32Color = pstFrmInfo->u32BorderColor;
    stBorder.stBorder.u32TopWidth = pstFrmInfo->u32BorderTopWidth;
    stBorder.stBorder.u32BottomWidth = pstFrmInfo->u32BorderTopWidth;
    stBorder.stBorder.u32LeftWidth = pstFrmInfo->u32BorderLeftWidth;
    stBorder.stBorder.u32RightWidth = pstFrmInfo->u32BorderRightWidth;
    stBorder.bBorderEn = RK_TRUE;
    ret = RK_MPI_VO_SetChnBorder(voLayer, pstFrmInfo->u32ChnnNum, &stBorder);
    if (ret) {
        RK_LOGD("RK_MPI_VO_SetChnBorder failed(%d)", ret);
        return RK_FAILURE;
    }

    RK_MPI_VO_SetChnFrameRate(voLayer, pstFrmInfo->u32ChnnNum, 30);

    ret = RK_MPI_VO_EnableChn(voLayer, pstFrmInfo->u32ChnnNum);
    if (ret) {
        RK_LOGD("RK_MPI_VO_EnableChn failed(%d)", ret);
        return RK_FAILURE;
    }

    return ret;
}

void RKVideoPlayer::replay() {
    //(reinterpret_cast<RTMediaBuffer*>(pCbMblk))->addRefs();

    VO_LAYER voLayer;
    switch (pstFrmInfo->u32VoDev) {
    case VO_DEV_HD0:
        voLayer = VOP_LAYER_CLUSTER_0;
        break;
    case VO_DEV_HD1:
        voLayer = VOP_LAYER_CLUSTER_1;
        break;
    default:
        voLayer = VOP_LAYER_CLUSTER_0;
        break;
    }

    usleep(30*1000);
    RK_MPI_VO_ClearChnBuffer(voLayer, pstFrmInfo->u32ChnnNum, RK_TRUE);
    //(reinterpret_cast<RTMediaBuffer*>(pCbMblk))->signalBufferRelease(RK_TRUE);

    s32Flag = 0;
}

RKVideoPlayer::RKVideoPlayer() {
    pstFrmInfo = rt_malloc(VIDEO_FRAMEINFO_S);
    rt_memset(pstFrmInfo, 0, sizeof(VIDEO_FRAMEINFO_S));
    s32Flag = 0;
}

RKVideoPlayer::~RKVideoPlayer() {
    rt_safe_free(pstFrmInfo);
}

INT32 RKVideoPlayer::queueBuffer(void *buf, INT32 fence) {
    VIDEO_FRAME_INFO_S stVFrameInfo;
    VO_LAYER voLayer;
    int ret, error;
    RTMediaBuffer *pstMBbuffer;
    RtMetaData *meta;

    RK_CHECK_POINTER(buf, RK_FAILURE);
    pCbMblk = buf;

    pstMBbuffer = reinterpret_cast<RTMediaBuffer*>(buf);
    meta = pstMBbuffer->getMetaData();
    meta->findInt32(kKeyFrameError, &error);
    if (error) {
      RK_LOGE("frame error");
      return -1;
    }

    if (!meta->findInt32(kKeyFrameW, reinterpret_cast<int *>(&stVFrameInfo.stVFrame.u32VirWidth))) {
      RK_LOGE("not find width stride in meta");
      return -1;
    }
    if (!meta->findInt32(kKeyFrameH, reinterpret_cast<int *>(&stVFrameInfo.stVFrame.u32VirHeight))) {
      RK_LOGE("not find height stride in meta");
      return -1;
    }

    if (!meta->findInt32(kKeyVCodecWidth, reinterpret_cast<int *>(&stVFrameInfo.stVFrame.u32Width))) {
      RK_LOGE("not find width in meta");
      return -1;
    }
    if (!meta->findInt32(kKeyVCodecHeight, reinterpret_cast<int *>(&stVFrameInfo.stVFrame.u32Height))) {
      RK_LOGE("not find height in meta");
      return -1;
    }

    /* Set Layer */
    stVFrameInfo.stVFrame.pMbBlk = buf;

    switch (pstFrmInfo->u32VoDev) {
    case VO_DEV_HD0:
        voLayer = VOP_LAYER_CLUSTER_0;
        break;
    case VO_DEV_HD1:
        voLayer = VOP_LAYER_CLUSTER_1;
        break;
    default:
        voLayer = VOP_LAYER_CLUSTER_0;
        break;
    }

    if (!s32Flag) {
        RTFrame frame = {0};
        VO_PUB_ATTR_S stVoPubAttr;
        VO_VIDEO_LAYER_ATTR_S stLayerAttr;
        void *pMblk = RK_NULL;

        RK_LOGD("stVFrame[%d, %d, %d, %d]", stVFrameInfo.stVFrame.u32Width,
                                            stVFrameInfo.stVFrame.u32Height,
                                            stVFrameInfo.stVFrame.u32VirWidth,
                                            stVFrameInfo.stVFrame.u32VirHeight);

        rt_memset(&stVoPubAttr, 0, sizeof(VO_PUB_ATTR_S));
        pMblk = buf;

        /* Bind Layer */
        VO_LAYER_MODE_E mode;
        switch (pstFrmInfo->u32VoLayerMode) {
        case 0:
            mode = VO_LAYER_MODE_CURSOR;
            break;
        case 1:
            mode = VO_LAYER_MODE_GRAPHIC;
            break;
        case 2:
            mode = VO_LAYER_MODE_VIDEO;
            break;
        default:
            mode = VO_LAYER_MODE_VIDEO;
        }
        ret = RK_MPI_VO_BindLayer(voLayer, pstFrmInfo->u32VoDev, mode);
        if (ret) {
            RK_LOGD("RK_MPI_VO_BindLayer failed(%d)", ret);
            return ret;
        }

        /* Enable VO Device */
        switch (pstFrmInfo->u32EnIntfType) {
        case DISPLAY_TYPE_HDMI:
            stVoPubAttr.enIntfType = VO_INTF_HDMI;
            break;
        case DISPLAY_TYPE_EDP:
            stVoPubAttr.enIntfType = VO_INTF_EDP;
            break;
        case DISPLAY_TYPE_VGA:
            stVoPubAttr.enIntfType = VO_INTF_VGA;
            break;
        case DISPLAY_TYPE_MIPI:
            stVoPubAttr.enIntfType = VO_INTF_MIPI;
            break;
        default:
            stVoPubAttr.enIntfType = VO_INTF_HDMI;
            RK_LOGD("option not set ,use HDMI default");
        }

        stVoPubAttr.enIntfSync = pstFrmInfo->enIntfSync;
        if (VO_OUTPUT_USER == stVoPubAttr.enIntfSync)
            RKCASE_VO_SetRtSyncInfo(&stVoPubAttr.stSyncInfo, pstFrmInfo);

        ret = RKCASE_VO_StartDev(pstFrmInfo->u32VoDev, &stVoPubAttr);
        if (ret) {
            RK_LOGD("RKCASE_VO_StartDev failed(%d)", ret);
            return ret;
        }

        RK_LOGD("StartDev");

        /* Enable Layer */
        stLayerAttr.enPixFormat = RKCASE_FmtToRtfmt(pstFrmInfo->u32VoFormat);
        stLayerAttr.stDispRect.s32X = 0;
        stLayerAttr.stDispRect.s32Y = 0;
        stLayerAttr.stDispRect.u32Width = 720;
        stLayerAttr.stDispRect.u32Height = 1280;
        stLayerAttr.stImageSize.u32Width = 720;
        stLayerAttr.stImageSize.u32Height = 1280;

        if (VO_OUTPUT_DEFAULT == stVoPubAttr.enIntfSync) {
            ret = RKCASE_VO_SetDispRect_Default(&stLayerAttr, pstFrmInfo->u32VoDev);

            RK_LOGD("width = %u, height = %u\n", stLayerAttr.stDispRect.u32Width, stLayerAttr.stDispRect.u32Height);

            if (ret) {
                RK_LOGD("SetDispRect_Default failed(%d)", ret);
                return ret;
            }
        }

        RKCASE_VO_SetDispRect(&stLayerAttr, pstFrmInfo);
        stLayerAttr.u32DispFrmRt = pstFrmInfo->u32DispFrmRt;
        ret = RKCASE_VO_StartLayer(voLayer, &stLayerAttr);
        if (ret) {
            RK_LOGD("RKCASE_VO_StartLayer failed(%d)", ret);
            return ret;
        }

          /* Enable channel */
        ret = RKCASE_VO_StartChnn(voLayer, &stLayerAttr, pstFrmInfo);
        if (ret) {
            RK_LOGD("RKCASE_VO_StartChnn failed(%d)", ret);
            return ret;
        }

        s32Flag = 1;
    }
    ret = RK_MPI_VO_SendFrame(voLayer, pstFrmInfo->u32ChnnNum, &stVFrameInfo, 0);
    if (ret) {
        RK_LOGD("RK_MPI_VO_SendLayerFrame failed(%d)", ret);
        return ret;
    }

    return RK_SUCCESS;
}
