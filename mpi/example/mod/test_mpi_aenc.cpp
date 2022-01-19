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
 *
 */
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <pthread.h>
#include "rk_debug.h"
#include "rk_mpi_aenc.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "argparse.h"

typedef struct _rkTEST_AENC_CTX_S {
    const char *srcFilePath;
    const char *dstFilePath;
    RK_S32      s32LoopCount;
    RK_S32      s32ChnNum;
    RK_S32      s32SampleRate;
    RK_S32      s32Channel;
    RK_S32      s32Format;
    char       *chCodecId;
    RK_S32      s32MilliSec;
    RK_S32      s32ChnIndex;
    RK_S32      s32FrameSize;
} TEST_AENC_CTX_S;

static RK_S32 aenc_data_free(void *opaque) {
    if (opaque) {
        free(opaque);
        opaque = RK_NULL;
    }

    return 0;
}

static RK_U32 test_find_audio_enc_codec_id(TEST_AENC_CTX_S *params) {
    if (params == RK_NULL)
        return -1;

    char *format = params->chCodecId;
    if (strstr(format, "mp2")) {
        return RK_AUDIO_ID_MP2;
    } else if (strstr(format, "g726")) {
        return RK_AUDIO_ID_ADPCM_G726;
    } else if (strstr(format, "g711a")) {
        return RK_AUDIO_ID_PCM_ALAW;
    } else if (strstr(format, "g711u")) {
        return RK_AUDIO_ID_PCM_MULAW;
    }
    RK_LOGE("test not find codec id : %s", params->chCodecId);
    return -1;
}

static RK_U32 test_find_audio_enc_format(TEST_AENC_CTX_S *params) {
    if (params == RK_NULL)
        return -1;

    if (params->s32Format == 8) {
        return AUDIO_BIT_WIDTH_8;
    } else if (params->s32Format == 16) {
        return AUDIO_BIT_WIDTH_16;
    } else if (params->s32Format == 24) {
        return AUDIO_BIT_WIDTH_24;
    }

    RK_LOGE("test not find format : %s", params->s32Format);
    return -1;
}

RK_S32 test_init_mpi_aenc(TEST_AENC_CTX_S *params) {
    RK_S32 s32ret = 0;
    AENC_CHN_ATTR_S stAencAttr;
    AENC_CHN AdChn = (AENC_CHN)(params->s32ChnIndex);

    RK_U32 codecId = test_find_audio_enc_codec_id(params);
    if (codecId == -1) {
        return RK_FAILURE;
    }

    RK_U32 format = test_find_audio_enc_format(params);
    if (codecId == -1) {
        return RK_FAILURE;
    }

    stAencAttr.enType = (RK_CODEC_ID_E)codecId;
    stAencAttr.stAencCodec.u32Channels = params->s32Channel;
    stAencAttr.stAencCodec.u32SampleRate = params->s32SampleRate;

    stAencAttr.stAencCodec.enBitwidth = (AUDIO_BIT_WIDTH_E)format;
    stAencAttr.u32BufCount = 4;
    stAencAttr.extraDataSize = 0;
    stAencAttr.extraData = RK_NULL;

    s32ret = RK_MPI_AENC_CreateChn(AdChn, &stAencAttr);
    if (s32ret) {
        RK_LOGE("create aenc chn %d err:0x%x\n", AdChn, s32ret);
        return RK_FAILURE;
    }

    return RK_SUCCESS;
}

static void *send_frame_thread(void *arg) {
    RK_S32 s32ret = 0;
    TEST_AENC_CTX_S *params = reinterpret_cast<TEST_AENC_CTX_S *>(arg);
    RK_U8 *srcData = RK_NULL;
    RK_S32 srcSize = 0;
    FILE  *file = RK_NULL;
    RK_S32 frameEos = 0;
    AUDIO_FRAME_S stAudioFrm;
    ADEC_CHN AdChn = (AENC_CHN)(params->s32ChnIndex);
    RK_U64 timeStamp = 0;
    RK_S32 count = 0;
    RK_S32 frmLen = params->s32FrameSize;

    file = fopen(params->srcFilePath, "rb");
    if (file == RK_NULL) {
        RK_LOGE("failed to open input file(%s), error: %s", params->srcFilePath, strerror(errno));
        goto __FAILED;
    }

    while (1) {
        srcData = reinterpret_cast<RK_U8 *>(calloc(frmLen, sizeof(RK_U8)));
        memset(srcData, 0, frmLen);

        srcSize = fread(srcData, 1, frmLen, file);

        if (srcSize == 0 || srcData == RK_NULL) {
            RK_LOGI("read eos frame, now send eos frame!");
            frameEos = 1;
        }
        RK_LOGV("send frame srcSize = %d, srcData = %p", srcSize, srcData);
        stAudioFrm.u32Len = srcSize;
        stAudioFrm.u64TimeStamp = timeStamp;
        stAudioFrm.u32Seq = ++count;
        stAudioFrm.bBypassMbBlk = RK_TRUE;

        MB_EXT_CONFIG_S extConfig = {0};
        extConfig.pFreeCB = aenc_data_free;
        extConfig.pOpaque = srcData;
        extConfig.pu8VirAddr = srcData;
        extConfig.u64Size    = srcSize;
        RK_MPI_SYS_CreateMB(&(stAudioFrm.pMbBlk), &extConfig);

        s32ret = RK_MPI_AENC_SendFrame(AdChn, &stAudioFrm, RK_NULL, params->s32MilliSec);
        if (s32ret != RK_SUCCESS) {
            RK_LOGV("fail to send aenc stream.");
        }
        RK_MPI_MB_ReleaseMB(stAudioFrm.pMbBlk);

        if (frameEos)
            break;
        timeStamp++;
    }

__FAILED:
    if (file) {
        fclose(file);
        file = RK_NULL;
    }

    return RK_NULL;
}

static void *receive_stream_thread(void *arg) {
    RK_S32 s32ret = 0;
    FILE *file = RK_NULL;
    TEST_AENC_CTX_S *params = reinterpret_cast<TEST_AENC_CTX_S *>(arg);
    AUDIO_STREAM_S pstStream;
    AENC_CHN AdChn = (AENC_CHN)(params->s32ChnIndex);
    RK_S32 eos = 0;
    RK_S32 count = 0;

    if (params->dstFilePath) {
        file = fopen(params->dstFilePath, "wb+");
        if (file == RK_NULL) {
            RK_LOGE("failed to open output file %s, error: %s.", params->dstFilePath, strerror(errno));
            goto __FAILED;
        }
    }

    while (1) {
        s32ret = RK_MPI_AENC_GetStream(AdChn, &pstStream, params->s32MilliSec);
        if (s32ret == RK_SUCCESS) {
            MB_BLK bBlk = pstStream.pMbBlk;
            RK_VOID *pstFrame = RK_MPI_MB_Handle2VirAddr(bBlk);
            RK_S32 frameSize = pstStream.u32Len;
            eos = (frameSize <= 0) ? 1 : 0;
            if (pstFrame) {
                RK_LOGV("get frame data = %p, size = %d", pstFrame, frameSize);
                if (file) {
                    fwrite(pstFrame, frameSize, 1, file);
                    fflush(file);
                }
                RK_MPI_AENC_ReleaseStream(AdChn, &pstStream);
                count++;
            }
        } else {
            RK_LOGE("fail to get aenc frame.");
        }

        if (eos) {
            RK_LOGI("get eos stream.");
            break;
        }
    }

__FAILED:
    if (file) {
        fclose(file);
        file = RK_NULL;
    }
    return RK_NULL;
}

RK_S32 unit_test_mpi_aenc(TEST_AENC_CTX_S *params) {
    RK_S32 i = 0;
    TEST_AENC_CTX_S aencCtx[AENC_MAX_CHN_NUM];
    pthread_t tidSend[AENC_MAX_CHN_NUM];
    pthread_t tidReceive[AENC_MAX_CHN_NUM];

    if (params->s32ChnNum > AENC_MAX_CHN_NUM) {
        RK_LOGE("aenc chn(%d) > max_chn(%d)", params->s32ChnNum, AENC_MAX_CHN_NUM);
        goto __FAILED;
    }

    for (i = 0; i < params->s32ChnNum; i++) {
        memcpy(&(aencCtx[i]), params, sizeof(TEST_AENC_CTX_S));
        aencCtx[i].s32ChnIndex = i;
        aencCtx[i].s32MilliSec = -1;

        if (test_init_mpi_aenc(&aencCtx[i]) == RK_FAILURE) {
            goto __FAILED;
        }

        pthread_create(&tidSend[i], RK_NULL, send_frame_thread, reinterpret_cast<void *>(&aencCtx[i]));
        pthread_create(&tidReceive[i], RK_NULL, receive_stream_thread, reinterpret_cast<void *>(&aencCtx[i]));
    }

    for (i = 0; i < params->s32ChnNum; i++) {
        pthread_join(tidSend[i], RK_NULL);
        pthread_join(tidReceive[i], RK_NULL);
        RK_MPI_AENC_DestroyChn((AENC_CHN)i);
    }

    return RK_SUCCESS;
__FAILED:

    return RK_FAILURE;
}

static const char *const usages[] = {
    "./rk_mpi_aenc_test [-i src_path] [-C name] [--input_rate rate] [--input_ch ch] [--input_format format]...",
    NULL,
};

static void mpi_aenc_test_show_options(const TEST_AENC_CTX_S *ctx) {
    RK_PRINT("cmd parse result:\n");
    RK_PRINT("input  file name       : %s\n", ctx->srcFilePath);
    RK_PRINT("output file name       : %s\n", ctx->dstFilePath);
    RK_PRINT("loop count             : %d\n", ctx->s32LoopCount);
    RK_PRINT("channel number         : %d\n", ctx->s32ChnNum);
    RK_PRINT("input sample rate      : %d\n", ctx->s32SampleRate);
    RK_PRINT("input channel          : %d\n", ctx->s32Channel);
    RK_PRINT("input format           : %d\n", ctx->s32Format);
    RK_PRINT("input codec name       : %s\n", ctx->chCodecId);
}

int main(int argc, const char **argv) {
    RK_S32           i;
    RK_S32           s32Ret;
    TEST_AENC_CTX_S *ctx;

    ctx = reinterpret_cast<TEST_AENC_CTX_S *>(malloc(sizeof(TEST_AENC_CTX_S)));
    memset(ctx, 0, sizeof(TEST_AENC_CTX_S));

    ctx->srcFilePath     = RK_NULL;
    ctx->dstFilePath     = RK_NULL;
    ctx->s32LoopCount    = 1;
    ctx->s32ChnNum       = 1;
    ctx->chCodecId       = RK_NULL;
    ctx->s32Format       = 16;
    ctx->s32FrameSize    = 1024;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("basic options:"),
        OPT_STRING('i', "input",  &(ctx->srcFilePath),
                   "input file name , e.g.(./*.mp3). <required>", NULL, 0, 0),
        OPT_STRING('C', "codec", &(ctx->chCodecId),
                    "codec, e.g.(mp2/g711a/g711u/g726). <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_ch", &(ctx->s32Channel),
                    "the number of input stream channels. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_rate", &(ctx->s32SampleRate),
                    "the sample rate of input stream. <required>", NULL, 0, 0),
        OPT_INTEGER('\0', "input_format", &(ctx->s32Format),
                    "the input format of input stream. <required>", NULL, 0, 0),
        OPT_STRING('o', "output", &(ctx->dstFilePath),
                    "output file name, e.g.(./*.pcm). default(NULL).", NULL, 0, 0),
        OPT_INTEGER('n', "loop_count", &(ctx->s32LoopCount),
                    "loop running count. default(1)", NULL, 0, 0),
        OPT_INTEGER('c', "channel_count", &(ctx->s32ChnNum),
                    "the count of adec channel. default(1).", NULL, 0, 0),
        OPT_INTEGER('\0', "frame_size", &(ctx->s32FrameSize),
                    "the size of send frame. default(1024).", NULL, 0, 0),
        OPT_END(),
    };

    struct argparse argparse;
    argparse_init(&argparse, options, usages, 0);
    argparse_describe(&argparse, "\nselect a test case to run.",
                                 "\nuse --help for details.");

    argc = argparse_parse(&argparse, argc, argv);
    mpi_aenc_test_show_options(ctx);

    // must set params
    if (ctx->srcFilePath == RK_NULL
        || ctx->s32Channel <= 0
        || ctx->s32SampleRate <= 0
        || ctx->chCodecId == RK_NULL) {
        argparse_usage(&argparse);
        goto __FAILED;
    }
    RK_MPI_SYS_Init();
    for (i = 0; i < ctx->s32LoopCount; i++) {
        RK_LOGI("start running loop count  = %d", i);
        s32Ret = unit_test_mpi_aenc(ctx);
        if (s32Ret != RK_SUCCESS) {
            goto __FAILED;
        }
        RK_LOGI("end running loop count  = %d", i);
    }

__FAILED:
    if (ctx) {
        free(ctx);
        ctx = RK_NULL;
    }
    RK_MPI_SYS_Exit();
    return 0;
}
