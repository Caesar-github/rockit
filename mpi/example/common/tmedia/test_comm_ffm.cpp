/*
 * Copyright 2020 Rockchip Electronics Co. LTD
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

#include "test_comm_ffm.h"

#include "rk_common.h"

#define SIZE_ARRAY_ELEMS(a)          (sizeof(a) / sizeof((a)[0]))

typedef struct _rkCodecInfo {
    RK_CODEC_ID_E enRkCodecId;
    enum AVCodecID enAvCodecId;
    char mine[16];
} CODEC_INFO;

static CODEC_INFO gCodecMapList[] = {
    { RK_VIDEO_ID_MPEG1VIDEO,    AV_CODEC_ID_MPEG1VIDEO,  "mpeg1" },
    { RK_VIDEO_ID_MPEG2VIDEO,    AV_CODEC_ID_MPEG2VIDEO,  "mpeg2" },
    { RK_VIDEO_ID_H263,          AV_CODEC_ID_H263,  "h263"  },
    { RK_VIDEO_ID_MPEG4,         AV_CODEC_ID_MPEG4, "mpeg4"  },
    { RK_VIDEO_ID_WMV,           AV_CODEC_ID_WMV3,  "wmv3" },
    { RK_VIDEO_ID_AVC,           AV_CODEC_ID_H264,  "h264" },
    { RK_VIDEO_ID_MJPEG,         AV_CODEC_ID_MJPEG, "mjpeg" },
    { RK_VIDEO_ID_VP8,           AV_CODEC_ID_VP8,   "vp8" },
    { RK_VIDEO_ID_VP9,           AV_CODEC_ID_VP9,   "vp9" },
    { RK_VIDEO_ID_HEVC,          AV_CODEC_ID_HEVC,  "hevc" },
    { RK_VIDEO_ID_VC1,           AV_CODEC_ID_VC1,   "vc1" },
    { RK_VIDEO_ID_AVS,           AV_CODEC_ID_AVS,   "avs" },
    { RK_VIDEO_ID_AVS,           AV_CODEC_ID_CAVS,  "cavs" },
    { RK_VIDEO_ID_AVSPLUS,       AV_CODEC_ID_CAVS,  "avs+" },
    { RK_VIDEO_ID_FLV1,          AV_CODEC_ID_FLV1,  "flv1" },
};

RK_S32 TEST_COMM_CodecIDFfmpegToRK(RK_S32 s32Id) {
    RK_BOOL bFound = RK_FALSE;
    RK_U32 i = 0;
    for (i = 0; i < SIZE_ARRAY_ELEMS(gCodecMapList); i++) {
        if (s32Id == gCodecMapList[i].enAvCodecId) {
            bFound = RK_TRUE;
            break;
        }
    }

    if (bFound)
        return gCodecMapList[i].enRkCodecId;
    else
        return RK_VIDEO_ID_Unused;
}
