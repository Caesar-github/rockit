/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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

#include <stdio.h>
#include "gtest/gtest.h"

#include "test_comm_sys.h"
#include "test_comm_vpss.h"
#include "test_comm_imgproc.h"
#include "test_comm_utils.h"

#include "rk_mpi_vpss.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_mb.h"

class RKSysBindTest : public ::testing::Test {
 public:
    RKSysBindTest() {}
    virtual ~RKSysBindTest() {}

 protected:
    virtual void SetUp() {
        memset(&mStVpssGrpAttr, 0, sizeof(VPSS_GRP_ATTR_S));
        memset(&mStVpssChnAttr, 0, sizeof(VPSS_CHN_ATTR_S));
        memset(&mStVideoFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

        mTimeOutMs = 100;

        mStVpssGrpAttr.u32MaxW = 4096;
        mStVpssGrpAttr.u32MaxH = 4096;
        mStVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
        mStVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;

        mStVpssChnAttr.enChnMode = VPSS_CHN_MODE_PASSTHROUGH;
        mStVpssChnAttr.u32Width = 1280;
        mStVpssChnAttr.u32Height = 720;
        mStVpssChnAttr.enPixelFormat = RK_FMT_YUV420SP;
        mStVpssChnAttr.stFrameRate.s32SrcFrameRate = -1;
        mStVpssChnAttr.stFrameRate.s32DstFrameRate = -1;
        mStVpssChnAttr.u32Depth = 8;

        PIC_BUF_ATTR_S stBufAttr;
        stBufAttr.u32Width = 1280;
        stBufAttr.u32Height = 720;
        stBufAttr.enPixelFormat = RK_FMT_YUV420SP;
        stBufAttr.enCompMode = COMPRESS_MODE_NONE;
        RK_S32 s32Ret = TEST_SYS_CreateVideoFrame(&stBufAttr, &mStVideoFrame);
        EXPECT_EQ(s32Ret, RK_SUCCESS);
        s32Ret = TEST_COMM_FillImage(
                            reinterpret_cast<RK_U8 *>(
                                RK_MPI_MB_Handle2VirAddr(mStVideoFrame.stVFrame.pMbBlk)),
                            mStVideoFrame.stVFrame.u32Width,
                            mStVideoFrame.stVFrame.u32Height,
                            mStVideoFrame.stVFrame.u32VirWidth,
                            mStVideoFrame.stVFrame.u32VirHeight,
                            mStVideoFrame.stVFrame.enPixelFormat,
                            1);
        EXPECT_EQ(s32Ret, RK_SUCCESS);
    }

    virtual void TearDown() {
        RK_S32 s32Ret = RK_MPI_SYS_MmzFree(mStVideoFrame.stVFrame.pMbBlk);
        EXPECT_EQ(s32Ret, RK_SUCCESS);
    }

    RK_S32 vpssBindVpss(VPSS_GRP VpssSrcGrp, VPSS_CHN VpssSrcChn, VPSS_GRP VpssDstGrp);
    RK_S32 vpssUnbindVpss(VPSS_GRP VpssSrcGrp, VPSS_CHN VpssSrcChn, VPSS_GRP VpssDstGrp);

 public:
    VPSS_GRP_ATTR_S mStVpssGrpAttr;
    VPSS_CHN_ATTR_S mStVpssChnAttr;
    VIDEO_FRAME_INFO_S mStVideoFrame;
    RK_S32 mTimeOutMs;
};

RK_S32 RKSysBindTest::vpssBindVpss(VPSS_GRP VpssSrcGrp, VPSS_CHN VpssSrcChn, VPSS_GRP VpssDstGrp) {
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId   = RK_ID_VPSS;
    stSrcChn.s32DevId  = VpssSrcGrp;
    stSrcChn.s32ChnId  = VpssSrcChn;

    stDestChn.enModId  = RK_ID_VPSS;
    stDestChn.s32DevId = VpssDstGrp;
    stDestChn.s32ChnId = 0;

    CHECK_RET(RK_MPI_SYS_Bind(&stSrcChn, &stDestChn), "RK_MPI_SYS_Bind(VPSS-VPSS)");

    return RK_SUCCESS;
}

RK_S32 RKSysBindTest::vpssUnbindVpss(VPSS_GRP VpssSrcGrp, VPSS_CHN VpssSrcChn, VPSS_GRP VpssDstGrp) {
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId   = RK_ID_VPSS;
    stSrcChn.s32DevId  = VpssSrcGrp;
    stSrcChn.s32ChnId  = VpssSrcChn;

    stDestChn.enModId  = RK_ID_VPSS;
    stDestChn.s32DevId = VpssDstGrp;
    stDestChn.s32ChnId = 0;

    CHECK_RET(RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn), "RK_MPI_SYS_UnBind(VPSS-VPSS)");

    return RK_SUCCESS;
}

TEST_F(RKSysBindTest, bindBeforeStartTest) {
    RK_S32 s32Ret = RK_SUCCESS;
    VPSS_GRP VpssSrcGrp = 0;
    VPSS_GRP VpssDstGrp = 1;
    VIDEO_FRAME_INFO_S stDstVideoFrame;

    memset(&stDstVideoFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    s32Ret = vpssBindVpss(VpssSrcGrp, 0, VpssDstGrp);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Start(VpssSrcGrp, 1, &mStVpssGrpAttr, &mStVpssChnAttr);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Start(VpssDstGrp, 1, &mStVpssGrpAttr, &mStVpssChnAttr);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = RK_MPI_VPSS_SendFrame(VpssSrcGrp, 0, &mStVideoFrame, mTimeOutMs);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = RK_MPI_VPSS_GetChnFrame(VpssDstGrp, 0, &stDstVideoFrame, mTimeOutMs);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    if (stDstVideoFrame.stVFrame.pMbBlk != RK_NULL) {
        EXPECT_EQ(TEST_COMM_CompareImageFuzzy(
                        reinterpret_cast<RK_U8 *>(
                            RK_MPI_MB_Handle2VirAddr(mStVideoFrame.stVFrame.pMbBlk)),
                        reinterpret_cast<RK_U8 *>(
                            RK_MPI_MB_Handle2VirAddr(stDstVideoFrame.stVFrame.pMbBlk)),
                        1280, 1280, 720, 0.1),
                RK_FALSE);
        s32Ret = RK_MPI_VPSS_ReleaseChnFrame(VpssDstGrp, 0, &stDstVideoFrame);
        EXPECT_EQ(s32Ret, RK_SUCCESS);
    }

    s32Ret = vpssUnbindVpss(VpssSrcGrp, 0, VpssDstGrp);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Stop(VpssSrcGrp, 1);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Stop(VpssDstGrp, 1);
    EXPECT_EQ(s32Ret, RK_SUCCESS);
}

TEST_F(RKSysBindTest, bindAfterStartTest) {
    RK_S32 s32Ret = RK_SUCCESS;
    VPSS_GRP VpssSrcGrp = 0;
    VPSS_GRP VpssDstGrp = 1;
    VIDEO_FRAME_INFO_S stDstVideoFrame;

    memset(&stDstVideoFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    s32Ret = TEST_VPSS_Start(VpssSrcGrp, 1, &mStVpssGrpAttr, &mStVpssChnAttr);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Start(VpssDstGrp, 1, &mStVpssGrpAttr, &mStVpssChnAttr);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = vpssBindVpss(VpssSrcGrp, 0, VpssDstGrp);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = RK_MPI_VPSS_SendFrame(VpssSrcGrp, 0, &mStVideoFrame, mTimeOutMs);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = RK_MPI_VPSS_GetChnFrame(VpssDstGrp, 0, &stDstVideoFrame, mTimeOutMs);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    if (stDstVideoFrame.stVFrame.pMbBlk != RK_NULL) {
        EXPECT_EQ(TEST_COMM_CompareImageFuzzy(
                        reinterpret_cast<RK_U8 *>(
                            RK_MPI_MB_Handle2VirAddr(mStVideoFrame.stVFrame.pMbBlk)),
                        reinterpret_cast<RK_U8 *>(
                            RK_MPI_MB_Handle2VirAddr(stDstVideoFrame.stVFrame.pMbBlk)),
                        1280, 1280, 720, 0.1),
                RK_FALSE);
        s32Ret = RK_MPI_VPSS_ReleaseChnFrame(VpssDstGrp, 0, &stDstVideoFrame);
        EXPECT_EQ(s32Ret, RK_SUCCESS);
    }

    s32Ret = vpssUnbindVpss(VpssSrcGrp, 0, VpssDstGrp);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Stop(VpssSrcGrp, 1);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Stop(VpssDstGrp, 1);
    EXPECT_EQ(s32Ret, RK_SUCCESS);
}

TEST_F(RKSysBindTest, sendFrameBeforeBindTest) {
    RK_S32 s32Ret = RK_SUCCESS;
    VPSS_GRP VpssSrcGrp = 0;
    VPSS_GRP VpssDstGrp = 1;
    VIDEO_FRAME_INFO_S stDstVideoFrame;

    memset(&stDstVideoFrame, 0, sizeof(VIDEO_FRAME_INFO_S));

    s32Ret = TEST_VPSS_Start(VpssSrcGrp, 1, &mStVpssGrpAttr, &mStVpssChnAttr);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Start(VpssDstGrp, 1, &mStVpssGrpAttr, &mStVpssChnAttr);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = RK_MPI_VPSS_SendFrame(VpssSrcGrp, 0, &mStVideoFrame, mTimeOutMs);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    // in order to frame process done
    usleep(100000);

    s32Ret = vpssBindVpss(VpssSrcGrp, 0, VpssDstGrp);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = RK_MPI_VPSS_GetChnFrame(VpssDstGrp, 0, &stDstVideoFrame, mTimeOutMs);
    EXPECT_NE(s32Ret, RK_SUCCESS);

    s32Ret = vpssUnbindVpss(VpssSrcGrp, 0, VpssDstGrp);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Stop(VpssSrcGrp, 1);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Stop(VpssDstGrp, 1);
    EXPECT_EQ(s32Ret, RK_SUCCESS);
}

TEST_F(RKSysBindTest, randomBindTest) {
    RK_S32 s32Ret = RK_SUCCESS;
    VPSS_GRP VpssSrcGrp = 0;
    VPSS_GRP VpssDstGrp = 1;
    RK_U32 u32RandomCnt = 100;
    RK_BOOL bBinding = RK_FALSE;
    VIDEO_FRAME_INFO_S stDstVideoFrame;
    RK_U32 u32Seed = 123;

    s32Ret = TEST_VPSS_Start(VpssSrcGrp, 1, &mStVpssGrpAttr, &mStVpssChnAttr);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Start(VpssDstGrp, 1, &mStVpssGrpAttr, &mStVpssChnAttr);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    while (u32RandomCnt-- > 0) {
        RK_S32 s32RandSleepUs = ((rand_r(&u32Seed)) % 5) * 20000;
        if ((s32RandSleepUs / 20000) % 2 == 0) {
            s32Ret = vpssBindVpss(VpssSrcGrp, 0, VpssDstGrp);
            if (bBinding == RK_FALSE) {
                EXPECT_EQ(s32Ret, RK_SUCCESS);
                bBinding = RK_TRUE;
            } else {
                EXPECT_NE(s32Ret, RK_SUCCESS);
            }
        } else {
            s32Ret = vpssUnbindVpss(VpssSrcGrp, 0, VpssDstGrp);
            if (bBinding == RK_TRUE) {
                EXPECT_EQ(s32Ret, RK_SUCCESS);
                bBinding = RK_FALSE;
            } else {
                EXPECT_NE(s32Ret, RK_SUCCESS);
            }
        }
        memset(&stDstVideoFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
        if (bBinding == RK_FALSE) {
            s32Ret = RK_MPI_VPSS_SendFrame(VpssSrcGrp, 0, &mStVideoFrame, mTimeOutMs);
            EXPECT_EQ(s32Ret, RK_SUCCESS);
            s32Ret = RK_MPI_VPSS_GetChnFrame(VpssDstGrp, 0, &stDstVideoFrame, mTimeOutMs);
            EXPECT_NE(s32Ret, RK_SUCCESS);
        } else {
            s32Ret = RK_MPI_VPSS_SendFrame(VpssSrcGrp, 0, &mStVideoFrame, mTimeOutMs);
            EXPECT_EQ(s32Ret, RK_SUCCESS);
            s32Ret = RK_MPI_VPSS_GetChnFrame(VpssDstGrp, 0, &stDstVideoFrame, mTimeOutMs);
            EXPECT_EQ(s32Ret, RK_SUCCESS);
            if (stDstVideoFrame.stVFrame.pMbBlk != RK_NULL) {
                EXPECT_EQ(TEST_COMM_CompareImageFuzzy(
                                reinterpret_cast<RK_U8 *>(
                                    RK_MPI_MB_Handle2VirAddr(mStVideoFrame.stVFrame.pMbBlk)),
                                reinterpret_cast<RK_U8 *>(
                                    RK_MPI_MB_Handle2VirAddr(stDstVideoFrame.stVFrame.pMbBlk)),
                                1280, 1280, 720, 0.1),
                        RK_FALSE);
                s32Ret = RK_MPI_VPSS_ReleaseChnFrame(VpssDstGrp, 0, &stDstVideoFrame);
                EXPECT_EQ(s32Ret, RK_SUCCESS);
            }
        }
        usleep(s32RandSleepUs);
    }

    s32Ret = TEST_VPSS_Stop(VpssSrcGrp, 1);
    EXPECT_EQ(s32Ret, RK_SUCCESS);

    s32Ret = TEST_VPSS_Stop(VpssDstGrp, 1);
    EXPECT_EQ(s32Ret, RK_SUCCESS);
}
