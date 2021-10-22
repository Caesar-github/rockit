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
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#include "test_comm_sys.h"
#include "test_comm_utils.h"

#include "rk_common.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_cal.h"

RK_S32 TEST_SYS_CreateVideoFrame(const PIC_BUF_ATTR_S *pstBufAttr, VIDEO_FRAME_INFO_S *pstVideoFrame) {
    MB_PIC_CAL_S stCalResult;

    if (pstBufAttr == RK_NULL || pstVideoFrame == RK_NULL) {
        return RK_FAILURE;
    }

    CHECK_RET(RK_MPI_CAL_VGS_GetPicBufferSize(pstBufAttr, &stCalResult),
             "VGS get pic size");

    CHECK_RET(RK_MPI_SYS_MmzAlloc_Cached(&(pstVideoFrame->stVFrame.pMbBlk),
                                        RK_NULL, RK_NULL, stCalResult.u32MBSize),
             "RK_MPI_SYS_MmzAlloc_Cached");

    pstVideoFrame->stVFrame.u32Width = pstBufAttr->u32Width;
    pstVideoFrame->stVFrame.u32Height = pstBufAttr->u32Height;
    pstVideoFrame->stVFrame.u32VirWidth = stCalResult.u32VirWidth;
    pstVideoFrame->stVFrame.u32VirHeight = stCalResult.u32VirHeight;
    pstVideoFrame->stVFrame.enPixelFormat = pstBufAttr->enPixelFormat;
    pstVideoFrame->stVFrame.enCompressMode = pstBufAttr->enCompMode;

    return RK_SUCCESS;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
