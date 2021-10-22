/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "rk_debug.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_cal.h"

#include "test_comm_argparse.h"
#include "test_comm_imgproc.h"
#include "test_comm_venc.h"

#define MAX_TIME_OUT_MS          20
#define TEST_RC_MODE             0
#define TEST_CROP_PARAM_CHANGE   0
#define TEST_SUPER_FRM           0
#define TEST_FRM_LOST            0
#define TEST_INTRA_REFRESH       0
#define TEST_HIERARCHICAL_QP     0
#define TEST_MJPEG_PARAM         0

typedef struct _rkMpiVENCCtx {
    const char     *srcFileUri;
    const char     *dstFilePath;
    RK_U32          u32SrcWidth;
    RK_U32          u32SrcHeight;
    RK_U32          u32srcVirWidth;
    RK_U32          u32srcVirHeight;
    RK_S32          s32LoopCount;
    RK_U32          u32ChnIndex;
    RK_U32          u32ChNum;
    RK_U32          u32SrcPixFormat;
    RK_U32          u32DstCodec;
    RK_U32          u32BufferSize;
    RK_U32          u32StreamBufCnt;
    RK_U32          u32BitRateKb;
    RK_U32          u32GopSize;
    RK_S32          s32SnapPicCount;
    RK_BOOL         threadExit;
    MB_POOL         vencPool;
    RK_BOOL         bFrameRate;
    RK_BOOL         bInsertUserData;
    COMPRESS_MODE_E enCompressMode;
    VENC_RC_MODE_E  enRcMode;
    VENC_GOP_MODE_E enGopMode;
    VENC_CROP_TYPE_E enCropType;
    ROTATION_E enRotation;
} TEST_VENC_CTX_S;

static RK_S32 read_with_pixel_width(RK_U8 *pBuf, RK_U32 u32Width, RK_U32 u32Height,
                                     RK_U32 u32VirWidth, RK_U32 u32PixWidth, FILE *fp) {
    RK_U32 u32Row;
    RK_S32 s32ReadSize;

    for (u32Row = 0; u32Row < u32Height; u32Row++) {
        s32ReadSize = fread(pBuf + u32Row * u32VirWidth * u32PixWidth, 1, u32Width * u32PixWidth, fp);
        if (s32ReadSize != u32Width * u32PixWidth) {
            RK_LOGE("read file failed expect %d vs %d\n",
                      u32Width * u32PixWidth, s32ReadSize);
            return RK_FAILURE;
        }
    }

    return RK_SUCCESS;
}

static RK_S32 read_image(RK_U8 *pVirAddr, RK_U32 u32Width, RK_U32 u32Height,
                                  RK_U32 u32VirWidth, RK_U32 u32VirHeight, RK_U32 u32PixFormat, FILE *fp) {
    RK_U32 u32Row = 0;
    RK_U32 u32ReadSize = 0;
    RK_S32 s32Ret = RK_SUCCESS;

    RK_U8 *pBufy = pVirAddr;
    RK_U8 *pBufu = pBufy + u32VirWidth * u32VirHeight;
    RK_U8 *pBufv = pBufu + u32VirWidth * u32VirHeight / 4;

    switch (u32PixFormat) {
        case RK_FMT_YUV420SP: {
            for (u32Row = 0; u32Row < u32Height; u32Row++) {
                u32ReadSize = fread(pBufy + u32Row * u32VirWidth, 1, u32Width, fp);
                if (u32ReadSize != u32Width) {
                     return RK_FAILURE;
                }
            }

            for (u32Row = 0; u32Row < u32Height / 2; u32Row++) {
                u32ReadSize = fread(pBufu + u32Row * u32VirWidth, 1, u32Width, fp);
                if (u32ReadSize != u32Width) {
                    return RK_FAILURE;
                }
            }
        } break;
        case RK_FMT_RGB888:
        case RK_FMT_BGR888: {
            s32Ret = read_with_pixel_width(pBufy, u32Width, u32Height, u32VirWidth, 3, fp);
        } break;
        default : {
            RK_LOGE("read image do not support fmt %d\n", u32PixFormat);
            return RK_FAILURE;
        } break;
    }

    return s32Ret;
}

static RK_S32 check_options(const TEST_VENC_CTX_S *ctx) {
    if (ctx->u32SrcPixFormat == RK_FMT_BUTT ||
        ctx->u32DstCodec <= RK_VIDEO_ID_Unused ||
        ctx->u32SrcWidth <= 0 ||
        ctx->u32SrcHeight <= 0) {
        goto __FAILED;
    }

    return RK_SUCCESS;

__FAILED:
    return RK_ERR_VENC_ILLEGAL_PARAM;
}

void* venc_get_stream(void *pArgs) {
    TEST_VENC_CTX_S *pstCtx     = reinterpret_cast<TEST_VENC_CTX_S *>(pArgs);
    void            *pData      = RK_NULL;
    RK_S32           s32Ret     = RK_SUCCESS;
    FILE            *fp         = RK_NULL;
    char             name[256]  = {0};
    RK_U32           u32Ch      = pstCtx->u32ChnIndex;
    RK_S32           s32StreamCnt = 0;
    VENC_STREAM_S    stFrame;

    if (pstCtx->dstFilePath != RK_NULL) {
        mkdir(pstCtx->dstFilePath, 0777);

        snprintf(name, sizeof(name), "%s/test_%d.bin",
            pstCtx->dstFilePath, pstCtx->u32ChnIndex);

        fp = fopen(name, "wb");
        if (fp == RK_NULL) {
            RK_LOGE("chn %d can't open file %s in get picture thread!\n", u32Ch, name);
            return RK_NULL;
        }
    }
    stFrame.pstPack = reinterpret_cast<VENC_PACK_S *>(malloc(sizeof(VENC_PACK_S)));

    while (!pstCtx->threadExit) {
        s32Ret = RK_MPI_VENC_GetStream(u32Ch, &stFrame, -1);
        if (s32Ret >= 0) {
            s32StreamCnt++;
            RK_LOGI("get chn %d stream %d", u32Ch, s32StreamCnt);
            if (pstCtx->dstFilePath != RK_NULL) {
                pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
                fwrite(pData, 1, stFrame.pstPack->u32Len, fp);
                fflush(fp);
            }
            RK_MPI_VENC_ReleaseStream(u32Ch, &stFrame);
            if (stFrame.pstPack->bStreamEnd == RK_TRUE) {
                RK_LOGI("chn %d reach EOS stream", u32Ch);
                break;
            }
        } else {
             if (pstCtx->threadExit) {
                break;
             }

             usleep(1000llu);
        }
    }

    if (stFrame.pstPack)
        free(stFrame.pstPack);

    if (fp)
        fclose(fp);

    return RK_NULL;
}

void* venc_send_frame(void *pArgs) {
    TEST_VENC_CTX_S     *pstCtx        = reinterpret_cast<TEST_VENC_CTX_S *>(pArgs);
    RK_S32               s32Ret         = RK_SUCCESS;
    RK_U8               *pVirAddr       = RK_NULL;
    FILE                *fp             = RK_NULL;
    MB_BLK               blk            = RK_NULL;
    RK_S32               s32LoopCount   = pstCtx->s32LoopCount;
    MB_POOL              pool           = pstCtx->vencPool;
    RK_U32               u32Ch          = pstCtx->u32ChnIndex;
    RK_S32               s32FrameCount  = 0;
    RK_S32               s32ReachEOS    = 0;
    VIDEO_FRAME_INFO_S   stFrame;

    memset(&stFrame, 0, sizeof(VIDEO_FRAME_INFO_S));



    while (!pstCtx->threadExit) {
        blk = RK_MPI_MB_GetMB(pool, pstCtx->u32BufferSize, RK_TRUE);

        if (RK_NULL == blk) {
            usleep(2000llu);
            continue;
        }
        pVirAddr = reinterpret_cast<RK_U8 *>(RK_MPI_MB_Handle2VirAddr(blk));

        if (pstCtx->srcFileUri) {
            if (fp == RK_NULL) {
                fp = fopen(pstCtx->srcFileUri, "r");
                if (fp == RK_NULL) {
                    RK_LOGE("chn %d can't open file %s!\n", u32Ch, pstCtx->srcFileUri);
                    return RK_NULL;
                }
            }
            if (pstCtx->enCompressMode == COMPRESS_AFBC_16x16) {
                if (fread(pVirAddr, 1, pstCtx->u32BufferSize, fp) != pstCtx->u32BufferSize)
                    s32Ret = RK_FAILURE;
                else
                    s32Ret = RK_SUCCESS;
            } else {
                s32Ret = read_image(pVirAddr, pstCtx->u32SrcWidth, pstCtx->u32SrcHeight,
                         pstCtx->u32srcVirWidth, pstCtx->u32srcVirHeight, pstCtx->u32SrcPixFormat, fp);
            }
        } else {
            s32Ret = TEST_COMM_FillImage(pVirAddr, pstCtx->u32SrcWidth,
                            pstCtx->u32SrcHeight,
                            pstCtx->u32srcVirWidth,
                            pstCtx->u32srcVirHeight,
                            (PIXEL_FORMAT_E)pstCtx->u32SrcPixFormat, s32FrameCount);
            if (s32Ret != RK_SUCCESS) {
                RK_MPI_MB_ReleaseMB(blk);
                RK_LOGE("TEST_COMM_FillImage fail ch %d", u32Ch);
                return RK_NULL;
            }
        }

        if (s32Ret != RK_SUCCESS) {
             s32ReachEOS = 1;
             if (s32LoopCount > 0) {
                s32LoopCount--;
                RK_LOGI("finish venc count %d\n", pstCtx->s32LoopCount - s32LoopCount);
                if (s32LoopCount > 0) {
                    s32ReachEOS = 0;
                    RK_MPI_MB_ReleaseMB(blk);

                    fseek(fp, 0L, SEEK_SET);
                    RK_LOGI("seek finish ch %d", u32Ch);
                    continue;
                }
             }
        }

#if TEST_CROP_PARAM_CHANGE
        if (s32FrameCount == 50) {
            VENC_CHN_PARAM_S stParam;
            RK_MPI_VENC_GetChnParam(u32Ch, &stParam);
            // for crop test
            if (stParam.stCropCfg.enCropType == VENC_CROP_ONLY) {
                stParam.stCropCfg.stCropRect.s32X = 100;
                stParam.stCropCfg.stCropRect.s32Y = 100;
                stParam.stCropCfg.stCropRect.u32Height = 400;
                stParam.stCropCfg.stCropRect.u32Width = 300;
            } else if (stParam.stCropCfg.enCropType == VENC_CROP_SCALE) {
            // for crop/scale test
                stParam.stCropCfg.stScaleRect.stSrc.s32X = 300;
                stParam.stCropCfg.stScaleRect.stSrc.s32Y = 500;
                stParam.stCropCfg.stScaleRect.stSrc.u32Width = stParam.stCropCfg.stScaleRect.stSrc.u32Width / 4;
                stParam.stCropCfg.stScaleRect.stSrc.u32Height = stParam.stCropCfg.stScaleRect.stSrc.u32Height;
                stParam.stCropCfg.stScaleRect.stDst.s32X = 0;
                stParam.stCropCfg.stScaleRect.stDst.s32Y = 0;
                stParam.stCropCfg.stScaleRect.stDst.u32Width = 400;
                stParam.stCropCfg.stScaleRect.stDst.u32Height = 400;
            }
            RK_MPI_VENC_SetChnParam(u32Ch, &stParam);
        }
#endif

        RK_MPI_SYS_MmzFlushCache(blk, RK_FALSE);

        stFrame.stVFrame.pMbBlk = blk;
        stFrame.stVFrame.u32Width = pstCtx->u32SrcWidth;
        stFrame.stVFrame.u32Height = pstCtx->u32SrcHeight;
        stFrame.stVFrame.u32VirWidth = pstCtx->u32srcVirWidth;
        stFrame.stVFrame.u32VirHeight = pstCtx->u32srcVirHeight;
        stFrame.stVFrame.enPixelFormat = (PIXEL_FORMAT_E)pstCtx->u32SrcPixFormat;
        stFrame.stVFrame.u32FrameFlag |= s32ReachEOS ? FRAME_FLAG_SNAP_END : 0;
        stFrame.stVFrame.enCompressMode = pstCtx->enCompressMode;

        if (pstCtx->bInsertUserData) {
            RK_U32 user_len;
            RK_U8 user_data[] = "this is rockchip user data";
            user_len = sizeof(user_data);
            RK_MPI_VENC_InsertUserData(u32Ch, user_data, user_len);
            RK_CHAR user_data2[1024];
            snprintf(user_data2, sizeof(user_data2), "rockchip continuous user data:%d", s32FrameCount);
            user_len = strlen(user_data2);
            RK_MPI_VENC_InsertUserData(u32Ch, reinterpret_cast<RK_U8 *>(user_data2), user_len);
        }
__RETRY:
        s32Ret = RK_MPI_VENC_SendFrame(u32Ch, &stFrame, -1);
        if (s32Ret < 0) {
            if (pstCtx->threadExit) {
                RK_MPI_MB_ReleaseMB(blk);
                break;
            }

            usleep(10000llu);
            goto  __RETRY;
        } else {
            RK_MPI_MB_ReleaseMB(blk);
            s32FrameCount++;
            RK_LOGI("chn %d frame %d", u32Ch, s32FrameCount);
        }
        if (s32ReachEOS ||
           (pstCtx->s32SnapPicCount != -1 && s32FrameCount >= pstCtx->s32SnapPicCount)) {
            RK_LOGI("chn %d reach EOS.", u32Ch);
            break;
        }
    }

    if (fp)
        fclose(fp);

    return RK_NULL;
}

RK_S32 unit_test_mpi_venc(TEST_VENC_CTX_S *ctx) {
    RK_S32                  s32Ret = RK_SUCCESS;
    RK_U32                  u32Ch = 0;
    VENC_CHN_ATTR_S         stAttr;
    VENC_CHN_PARAM_S        stParam;
    VENC_RECV_PIC_PARAM_S   stRecvParam;
    VENC_RC_PARAM_S         stRcParam;
    MB_POOL_CONFIG_S        stMbPoolCfg;
    TEST_VENC_CTX_S         stVencCtx[VENC_MAX_CHN_NUM];
    pthread_t               vencThread[VENC_MAX_CHN_NUM];
    pthread_t               getStreamThread[VENC_MAX_CHN_NUM];

    memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));
    memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
    memset(&stRcParam, 0, sizeof(VENC_RC_PARAM_S));
    memset(&stParam, 0, sizeof(VENC_CHN_PARAM_S));

    if (ctx->u32BufferSize <= 0) {
        PIC_BUF_ATTR_S stPicBufAttr;
        MB_PIC_CAL_S stMbPicCalResult;

        stPicBufAttr.u32Width = ctx->u32SrcWidth;
        stPicBufAttr.u32Height = ctx->u32SrcHeight;
        stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)ctx->u32SrcPixFormat;
        stPicBufAttr.enCompMode = ctx->enCompressMode;
        s32Ret = RK_MPI_CAL_COMM_GetPicBufferSize(&stPicBufAttr, &stMbPicCalResult);
        if (s32Ret != RK_SUCCESS) {
            RK_LOGE("get picture buffer size failed. err 0x%x", s32Ret);
            return s32Ret;
        }
        ctx->u32BufferSize = stMbPicCalResult.u32MBSize;
        RK_LOGD("calc picture size: %d", ctx->u32BufferSize);
    }

    if (ctx->u32BufferSize > 8192 * 8192 * 4) {
        RK_LOGE("too large picture size: %d", ctx->u32BufferSize);
        return RK_FAILURE;
    }

    for (u32Ch = 0; u32Ch < ctx->u32ChNum; u32Ch++) {
        if (ctx->u32ChNum >= 1) {
            ctx->u32ChnIndex = u32Ch;
        }

        stAttr.stRcAttr.enRcMode = ctx->enRcMode;
        stAttr.stRcAttr.stH264Cbr.u32Gop = ctx->u32GopSize;
        stAttr.stRcAttr.stH264Cbr.u32BitRate = ctx->u32BitRateKb;
        TEST_VENC_SET_BitRate(&stAttr.stRcAttr, ctx->u32BitRateKb);

        stAttr.stVencAttr.enType = (RK_CODEC_ID_E)ctx->u32DstCodec;
        stAttr.stVencAttr.u32Profile = H264E_PROFILE_HIGH;
        stAttr.stVencAttr.enPixelFormat = (PIXEL_FORMAT_E)ctx->u32SrcPixFormat;
        stAttr.stVencAttr.u32PicWidth = ctx->u32SrcWidth;
        stAttr.stVencAttr.u32PicHeight = ctx->u32SrcHeight;

        if (ctx->u32srcVirWidth <= 0) {
            ctx->u32srcVirWidth = ctx->u32SrcWidth;
        }
        stAttr.stVencAttr.u32VirWidth = ctx->u32srcVirWidth;

        if (ctx->u32srcVirHeight <= 0) {
            ctx->u32srcVirHeight = ctx->u32SrcHeight;
        }
        stAttr.stVencAttr.u32VirHeight = ctx->u32srcVirHeight;
        stAttr.stVencAttr.u32StreamBufCnt = ctx->u32StreamBufCnt;
        stAttr.stVencAttr.u32BufSize = ctx->u32BufferSize;

        if (stAttr.stVencAttr.enType == RK_VIDEO_ID_JPEG) {
            stAttr.stVencAttr.stAttrJpege.bSupportDCF = RK_FALSE;
            stAttr.stVencAttr.stAttrJpege.stMPFCfg.u8LargeThumbNailNum = 0;
            stAttr.stVencAttr.stAttrJpege.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
        }

        RK_MPI_VENC_CreateChn(u32Ch, &stAttr);

        stParam.stCropCfg.enCropType = ctx->enCropType;
        // for crop test
        if (stParam.stCropCfg.enCropType == VENC_CROP_ONLY) {
            stParam.stCropCfg.stCropRect.s32X = 10;
            stParam.stCropCfg.stCropRect.s32Y = 10;
            stParam.stCropCfg.stCropRect.u32Height = 100;
            stParam.stCropCfg.stCropRect.u32Width = 100;
        } else if (stParam.stCropCfg.enCropType == VENC_CROP_SCALE) {
        // for crop/scale test
            stParam.stCropCfg.stScaleRect.stSrc.s32X = 100;
            stParam.stCropCfg.stScaleRect.stSrc.s32Y = 200;
            stParam.stCropCfg.stScaleRect.stSrc.u32Width = ctx->u32SrcWidth / 2;
            stParam.stCropCfg.stScaleRect.stSrc.u32Height = ctx->u32SrcHeight / 2;
            stParam.stCropCfg.stScaleRect.stDst.s32X = 0;
            stParam.stCropCfg.stScaleRect.stDst.s32Y = 0;
            stParam.stCropCfg.stScaleRect.stDst.u32Width = 200;
            stParam.stCropCfg.stScaleRect.stDst.u32Height = 200;
        }
        // for framerate test
        {
            stParam.stFrameRate.bEnable = ctx->bFrameRate;
            stParam.stFrameRate.s32SrcFrmRateNum = 25;
            stParam.stFrameRate.s32SrcFrmRateDen = 1;
            stParam.stFrameRate.s32DstFrmRateNum = 10;
            stParam.stFrameRate.s32DstFrmRateDen = 1;
        }
        RK_MPI_VENC_SetChnParam(u32Ch, &stParam);

        if (ctx->enGopMode) {
            stAttr.stGopAttr.enGopMode = ctx->enGopMode;
            if (!stAttr.stRcAttr.stH264Cbr.u32Gop)
                stAttr.stRcAttr.stH264Cbr.u32Gop = 60;
            stAttr.stGopAttr.s32VirIdrLen = stAttr.stRcAttr.stH264Cbr.u32Gop / 2;
            RK_MPI_VENC_SetChnAttr(u32Ch, &stAttr);
        }

        if (ctx->enRotation) {
            RK_MPI_VENC_SetChnRotation(u32Ch, ctx->enRotation);
        }

        stRecvParam.s32RecvPicNum = ctx->s32SnapPicCount;
        RK_MPI_VENC_StartRecvFrame(u32Ch, &stRecvParam);

        if (stAttr.stVencAttr.enType == RK_VIDEO_ID_JPEG) {
            VENC_JPEG_PARAM_S stJpegParam;
            memset(&stJpegParam, 0, sizeof(stJpegParam));
            stJpegParam.u32Qfactor = 77;
            RK_MPI_VENC_SetJpegParam(u32Ch, &stJpegParam);
        }

#if TEST_RC_MODE
        stRcParam.s32FirstFrameStartQp = 25;
        stRcParam.stParamH264.u32StepQp = 4;
        stRcParam.stParamH264.u32MinQp = 10;
        stRcParam.stParamH264.u32MaxQp = 40;
        stRcParam.stParamH264.u32MinIQp = 20;
        stRcParam.stParamH264.u32MaxIQp = 30;
        stRcParam.stParamH264.s32DeltIpQp = -2;
        RK_MPI_VENC_SetRcParam(u32Ch, &stRcParam);
#endif

#if TEST_SUPER_FRM
        VENC_SUPERFRAME_CFG_S stSuperFrameCfg;
        memset(&stSuperFrameCfg, 0, sizeof(stSuperFrameCfg));
        stSuperFrameCfg.enSuperFrmMode = SUPERFRM_REENCODE;
        stSuperFrameCfg.u32SuperIFrmBitsThr = 100 * 1024 * 8;  // 100KByte
        stSuperFrameCfg.u32SuperPFrmBitsThr = 100 * 1024 * 8;  // 100KByte
        stSuperFrameCfg.enRcPriority = VENC_RC_PRIORITY_FRAMEBITS_FIRST;
        RK_MPI_VENC_SetSuperFrameStrategy(u32Ch, &stSuperFrameCfg);
#endif

#if TEST_INTRA_REFRESH
        VENC_INTRA_REFRESH_S stIntraRefresh;
        memset(&stIntraRefresh, 0, sizeof(stIntraRefresh));
        stIntraRefresh.bRefreshEnable = RK_TRUE;
        stIntraRefresh.enIntraRefreshMode = INTRA_REFRESH_ROW;
        stIntraRefresh.u32RefreshNum = 5;
        RK_MPI_VENC_SetIntraRefresh(u32Ch, &stIntraRefresh);
#endif

#if TEST_HIERARCHICAL_QP
        VENC_HIERARCHICAL_QP_S stHierarchicalQp;
        memset(&stHierarchicalQp, 0, sizeof(stHierarchicalQp));
        stHierarchicalQp.bHierarchicalQpEn = RK_TRUE;
        stHierarchicalQp.s32HierarchicalQpDelta[0] = 5;
        stHierarchicalQp.s32HierarchicalQpDelta[1] = 3;
        stHierarchicalQp.s32HierarchicalFrameNum[0] = 2;
        stHierarchicalQp.s32HierarchicalFrameNum[1] = 3;
        RK_MPI_VENC_SetHierarchicalQp(u32Ch, &stHierarchicalQp);
#endif

#if TEST_MJPEG_PARAM
        if (stAttr.stVencAttr.enType == RK_VIDEO_ID_MJPEG) {
            VENC_MJPEG_PARAM_S stMjpegParam;
            memset(&stMjpegParam, 0, sizeof(stMjpegParam));
            /* set qtable for test only, users need set actual qtable */
            memset(&stMjpegParam.u8YQt, 1, sizeof(stMjpegParam.u8YQt));
            memset(&stMjpegParam.u8CbQt, 1, sizeof(stMjpegParam.u8CbQt));
            memset(&stMjpegParam.u8CrQt, 1, sizeof(stMjpegParam.u8CrQt));
            stMjpegParam.u32MCUPerECS = 100;
            RK_MPI_VENC_SetMjpegParam(u32Ch, &stMjpegParam);
        }
#endif

#if TEST_FRM_LOST
        VENC_FRAMELOST_S stFrmLost;
        memset(&stFrmLost, 0, sizeof(stFrmLost));
        stFrmLost.bFrmLostOpen = RK_TRUE;
        stFrmLost.u32FrmLostBpsThr = 1;
        stFrmLost.enFrmLostMode = FRMLOST_NORMAL;
        stFrmLost.u32EncFrmGaps = 0;
        RK_MPI_VENC_SetFrameLostStrategy(u32Ch, &stFrmLost);
#endif

        memset(&stMbPoolCfg, 0, sizeof(MB_POOL_CONFIG_S));
        stMbPoolCfg.u64MBSize = ctx->u32BufferSize;
        stMbPoolCfg.u32MBCnt  = ctx->u32StreamBufCnt;
        stMbPoolCfg.enAllocType = MB_ALLOC_TYPE_DMA;

        ctx->vencPool = RK_MPI_MB_CreatePool(&stMbPoolCfg);

        memcpy(&(stVencCtx[u32Ch]), ctx, sizeof(TEST_VENC_CTX_S));
        pthread_create(&vencThread[u32Ch], 0, venc_send_frame, reinterpret_cast<void *>(&stVencCtx[u32Ch]));
        pthread_create(&getStreamThread[u32Ch], 0, venc_get_stream, reinterpret_cast<void *>(&stVencCtx[u32Ch]));
    }

    for (u32Ch = 0; u32Ch < ctx->u32ChNum; u32Ch++) {
        pthread_join(vencThread[u32Ch], RK_NULL);
        pthread_join(getStreamThread[u32Ch], RK_NULL);

        stVencCtx[u32Ch].threadExit = RK_TRUE;
        RK_MPI_VENC_StopRecvFrame(u32Ch);

        RK_MPI_VENC_DestroyChn(u32Ch);
        RK_MPI_MB_DestroyPool(stVencCtx[u32Ch].vencPool);
    }

    return RK_SUCCESS;
}

static const char *const usages[] = {
    "./rk_mpi_venc_test [-i SRC_PATH] [-w SRC_WIDTH] [-h SRC_HEIGHT]",
    NULL,
};

static void mpi_venc_test_show_options(const TEST_VENC_CTX_S *ctx) {
    RK_PRINT("cmd parse result:\n");
    RK_PRINT("input  file name       : %s\n", ctx->srcFileUri);
    RK_PRINT("output file name       : %s\n", ctx->dstFilePath);
    RK_PRINT("src width              : %d\n", ctx->u32SrcWidth);
    RK_PRINT("src height             : %d\n", ctx->u32SrcHeight);
    RK_PRINT("src virWidth           : %d\n", ctx->u32srcVirWidth);
    RK_PRINT("src virHeight          : %d\n", ctx->u32srcVirHeight);
    RK_PRINT("src pixel format       : %d\n", ctx->u32SrcPixFormat);
    RK_PRINT("encode codec type      : %d\n", ctx->u32DstCodec);
    RK_PRINT("loop count             : %d\n", ctx->s32LoopCount);
    RK_PRINT("channel index          : %d\n", ctx->u32ChnIndex);
    RK_PRINT("channel num            : %d\n", ctx->u32ChNum);
    RK_PRINT("output buffer count    : %d\n", ctx->u32StreamBufCnt);
    RK_PRINT("one picture size       : %d\n", ctx->u32BufferSize);
    RK_PRINT("gop mode               : %d\n", ctx->enGopMode);
    RK_PRINT("snap picture count     : %d\n", ctx->s32SnapPicCount);
    RK_PRINT("insert user data       : %d\n", ctx->bInsertUserData);
    RK_PRINT("rotation               : %d\n", ctx->enRotation);
    RK_PRINT("compress mode          : %d\n", ctx->enCompressMode);
    RK_PRINT("rc mode                : %d\n", ctx->enRcMode);
    RK_PRINT("bitrate                : %d\n", ctx->u32BitRateKb);
    RK_PRINT("gop size               : %d\n", ctx->u32GopSize);

    return;
}

int main(int argc, const char **argv) {
    RK_S32 s32Ret = RK_SUCCESS;
    TEST_VENC_CTX_S ctx;
    memset(&ctx, 0, sizeof(TEST_VENC_CTX_S));

    ctx.s32LoopCount    = 1;
    ctx.u32StreamBufCnt = 8;
    ctx.u32ChNum        = 1;
    ctx.u32SrcPixFormat = RK_FMT_YUV420SP;
    ctx.u32DstCodec     = RK_VIDEO_ID_AVC;
    ctx.enCropType      = VENC_CROP_NONE;
    ctx.bFrameRate      = RK_FALSE;
    ctx.s32SnapPicCount = -1;
    ctx.u32GopSize = 60;
    ctx.enRcMode = VENC_RC_MODE_H264CBR;
    ctx.u32BitRateKb = 10 * 1024;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("basic options:"),
        OPT_STRING('i', "input",  &(ctx.srcFileUri),
                   "input file name.", NULL, 0, 0),
        OPT_STRING('o', "output", &(ctx.dstFilePath),
                   "the directory of encoder output", NULL, 0, 0),
        OPT_INTEGER('n', "loop_count", &(ctx.s32LoopCount),
                    "loop running count. default(1)", NULL, 0, 0),
        OPT_INTEGER('w', "width", &(ctx.u32SrcWidth),
                    "input source width. <required>", NULL, 0, 0),
        OPT_INTEGER('h', "height", &(ctx.u32SrcHeight),
                    "input source height. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "vir_width", &(ctx.u32srcVirWidth),
                    "input source virWidth.", NULL, 0, 0),
        OPT_INTEGER('\0', "vir_height", &(ctx.u32srcVirHeight),
                    "input source virHeight.", NULL, 0, 0),
        OPT_INTEGER('f', "pixel_format", &(ctx.u32SrcPixFormat),
                    "input source pixel format. default(0: NV12).", NULL, 0, 0),
        OPT_INTEGER('C', "codec", &(ctx.u32DstCodec),
                     "venc encode codec(8:h264, 9:mjpeg, 12:h265, 15:jpeg, ...). default(8)", NULL, 0, 0),
        OPT_INTEGER('c', "channel_count", &(ctx.u32ChNum),
                     "venc channel count. default(1).", NULL, 0, 0),
        OPT_INTEGER('\0', "channel_index", &(ctx.u32ChnIndex),
                     "venc channel index. default(0).", NULL, 0, 0),
        OPT_INTEGER('\0', "enc_buf_cnt", &(ctx.u32StreamBufCnt),
                     "venc encode output buffer count, default(8)", NULL, 0, 0),
        OPT_INTEGER('\0', "src_pic_size", &(ctx.u32BufferSize),
                     "the size of input single picture", NULL, 0, 0),
        OPT_INTEGER('\0', "crop", &(ctx.enCropType),
                     "crop type(0:none 1:crop_only 2:crop_scale) default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "framerate", &(ctx.bFrameRate),
                     "framerate enable flag(0:disable 1:enable) default(0)", NULL, 0, 0),
        OPT_INTEGER('g', "gop_mode", &(ctx.enGopMode),
                    "gop mode(0/1:NORMALP 2:TSVC2 3:TSVC3 4:TSVC4 5:SMARTP) default(0)", NULL, 0, 0),
        OPT_INTEGER('s', "snap_pic_cnt", &(ctx.s32SnapPicCount),
                    "snap picture count(effective range[-1,0)(0, 2147483647)"
                    "-1:not limit else:snap count) default(-1)", NULL, 0, 0),
        OPT_INTEGER('\0', "insert_user_data", &(ctx.bInsertUserData),
                    "insert user data flag(0:disable 1:enable) default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "rotation", &(ctx.enRotation),
                     "rotation output(0:0 1:90 2:180 3:270) default(0)", NULL, 0, 0),
        OPT_INTEGER('\0', "compress_mode", &(ctx.enCompressMode),
                    "set input compressmode(default 0; 0:MODE_NONE 1:AFBC_16x16)", NULL, 0, 0),
        OPT_INTEGER('\0', "rc_mode", &(ctx.enRcMode),
                    "rc mode(0:NULL 1:H264CBR 2:H264VBR 3:H264AVBR 4:MJPEGCBR 5:MJPEGVBR "
                    "6:H265CBR 7:H265VBR 8:H265AVBR default(1)",
                    NULL, 0, 0),
        OPT_INTEGER('b', "bit_rate", &(ctx.u32BitRateKb),
                    "bit rate kbps(range[2-69905] default(10*1024kb))",
                    NULL, 0, 0),
        OPT_INTEGER('\0', "gop_size", &(ctx.u32GopSize),
                    "gop size(range >= 1 default(60))", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nselect a test case to run.",
                                 "\nuse --help for details.");

    argc = argparse_parse(&argparse, argc, argv);
    mpi_venc_test_show_options(&ctx);

    if (check_options(&ctx)) {
        argparse_usage(&argparse);
        return RK_FAILURE;
    }

    s32Ret = RK_MPI_SYS_Init();
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }

    if (unit_test_mpi_venc(&ctx) < 0) {
        goto __FAILED;
    }

    s32Ret = RK_MPI_SYS_Exit();
    if (s32Ret != RK_SUCCESS) {
        return s32Ret;
    }
    RK_LOGE("test running success!");
    return RK_SUCCESS;
__FAILED:
    RK_MPI_SYS_Exit();
    RK_LOGE("test running failed!");
    return s32Ret;
}
