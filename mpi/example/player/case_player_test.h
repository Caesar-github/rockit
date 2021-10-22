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

#ifndef __CASE_PLAYER_TEST__
#define __CASE_PLAYER_TEST__

#ifdef __cplusplus
extern "C" {
#endif

#include "rk_mpi_vo.h"
#include "rk_debug.h"

/** Error information of the player*/
typedef enum {
    RK_PLAYER_ERROR_VID_PLAY_FAIL = 0x0, /* < The video fails to be played. */
    RK_PLAYER_ERROR_AUD_PLAY_FAIL,       /* < The audio fails to be played. */
    RK_PLAYER_ERROR_DEMUX_FAIL,          /* < The file fails to be played. */
    RK_PLAYER_ERROR_TIMEOUT, /* < Operation timeout. For example, reading data timeout. */
    RK_PLAYER_ERROR_NOT_SUPPORT, /* < The file format is not supportted. */
    RK_PLAYER_ERROR_UNKNOW,      /* < Unknown error. */
    RK_PLAYER_ERROR_ILLEGAL_STATEACTION, /* < illegal action at cur state. */
    RK_PLAYER_ERROR_BUTT,
} RK_PLAYER_ERROR_E;

/** Player status*/
typedef enum {
    RK_PLAYER_STATE_IDLE = 0, /* < The player state before init . */
    RK_PLAYER_STATE_INIT, /* < The player is in the initial state. */
    RK_PLAYER_STATE_PREPARED, /* < The player is in the prepared state. */
    RK_PLAYER_STATE_PLAY,     /* < The player is in the playing state. */
    RK_PLAYER_STATE_TPLAY,    /* < The player is in the trick playing state*/
    RK_PLAYER_STATE_PAUSE,    /* < The player is in the pause state. */
    RK_PLAYER_STATE_ERR,      /* < The player is in the err state. */
    RK_PLAYER_STATE_BUTT
} RK_PLAYER_STATE_E;

typedef enum {
    RK_PLAYER_EVENT_STATE_CHANGED = 0x0, /* < the player status changed */
    RK_PLAYER_EVENT_PREPARED,
    RK_PLAYER_EVENT_STARTED,
    RK_PLAYER_EVENT_PAUSED,
    RK_PLAYER_EVENT_STOPPED,
    RK_PLAYER_EVENT_EOF, /* < the player is playing the end */
    RK_PLAYER_EVENT_SOF, /* < the player backward tplay to the start of file*/
    RK_PLAYER_EVENT_PROGRESS, /* < current playing progress. it will be called every one second. */
    RK_PLAYER_EVENT_SEEK_END, /* < seek time jump, the additional value is the seek value */
    RK_PLAYER_EVENT_ERROR,    /* < play error */
    RK_PLAYER_EVENT_BUTT
} RK_PLAYER_EVENT_E;

typedef enum {
    VO_FORMAT_ARGB8888 = 0,
    VO_FORMAT_ABGR8888,
    VO_FORMAT_RGB888,
    VO_FORMAT_BGR888,
    VO_FORMAT_ARGB1555,
    VO_FORMAT_ABGR1555,
    VO_FORMAT_NV12,
    VO_FORMAT_NV21
} RK_PLAYER_VO_FORMAT_E;

typedef enum {
    VO_DEV_HD0 = 0,
    VO_DEV_HD1
} RK_PLAYER_VO_DEV_E;

typedef enum {
    DISPLAY_TYPE_HDMI = 0,
    DISPLAY_TYPE_EDP,
    DISPLAY_TYPE_VGA,
    DISPLAY_TYPE_MIPI,
} RK_PLAYER_VO_INTF_TYPE_E;

typedef enum {
    CHNN_ASPECT_RATIO_AUTO = 1,
    CHNN_ASPECT_RATIO_MANUAL,
} RK_PLAYER_VO_CHNN_MODE_E;
typedef RK_VOID (*RK_PLAYER_EVENT_FN)(void *pPlayer,
                                      RK_PLAYER_EVENT_E enEvent,
                                      void *pData);

/** player configuration */
typedef struct {
    RK_BOOL bEnableVideo;
    RK_BOOL bEnableAudio;
    RK_PLAYER_EVENT_FN pfnPlayerCallback;
} RK_PLAYER_CFG_S;

/** video output frameinfo */
typedef struct {
    RK_U32 u32FrmInfoS32x;
    RK_U32 u32FrmInfoS32y;
    RK_U32 u32DispWidth;
    RK_U32 u32DispHeight;
    RK_U32 u32ImgWidth;
    RK_U32 u32ImgHeight;
    RK_U32 u32VoLayerMode;
    RK_U32 u32ChnnNum;
    RK_U32 u32BorderColor;
    RK_U32 u32BorderTopWidth;
    RK_U32 u32BorderBottomWidth;
    RK_U32 u32BorderLeftWidth;
    RK_U32 u32BorderRightWidth;
    RK_PLAYER_VO_CHNN_MODE_E u32EnMode;
    RK_PLAYER_VO_FORMAT_E u32VoFormat;
    RK_PLAYER_VO_DEV_E u32VoDev;
    RK_PLAYER_VO_INTF_TYPE_E u32EnIntfType;
    RK_U32 u32DispFrmRt;
    VO_INTF_SYNC_E enIntfSync;
    VO_SYNC_INFO_S stSyncInfo;
} RK_PLAYER_FRAMEINFO_S;

/** Pointer Check */
#define RK_CHECK_POINTER(p, errcode)                                        \
    do {                                                                         \
        if (!(p)) {                                                                \
            RK_LOGI("pointer[%s] is NULL", #p);                                   \
            return errcode;                                                          \
        }                                                                          \
    } while (0)

/** Pointer Check */
#define RK_CHECK_POINTER_N(p)                                        \
    do {                                                                         \
        if (!(p)) {                                                                \
            RK_LOGI("pointer[%s] is NULL", #p);                                   \
            return;                                                          \
        }                                                                          \
    } while (0)

/**
 * @brief create the player
 * @param[in] pstPlayCfg : player config
 * @param[out] ppPlayer : RKADK_MW_PTR*: handle of the player
 * @retval 0 success, others failed
 */
RK_S32 RK_PLAYER_Create(void **ppPlayer,
                            RK_PLAYER_CFG_S *pstPlayCfg);

/**
 * @brief  destroy the player
 * @param[in] pPlayer : RKADK_MW_PTR: handle of the player
 * @param[in] pSurface : RKADK_MW_PTR: handle of the pSurface
 *  @retval 0 success, others failed
 */
RK_S32 RK_PLAYER_Destroy(void *pPlayer, void *pSurface);

/**
 * @brief    set the file for playing
 * @param[in] pPlayer : RKADK_MW_PTR: handle of the player
 * @param[in] filePath : RKADK_CHAR: media file path
 * @retval  0 success, others failed
 */
RK_S32 RK_PLAYER_SetDataSource(void *pPlayer,
                                  const char *pszfilePath);

/**
 * @brief prepare for the playing
 * @param[in] pPlayer : RKADK_MW_PTR: handle of the player
 * @retval  0 success, others failed
 */
RK_S32 RK_PLAYER_Prepare(void *pPlayer);

/**
 * @brief setcallback for the playing
 * @param[in] pPlayer : RKADK_MW_PTR: handle of the player
 * @param[in] frameinfo : RKADK_FRAMEINFO_S: record displayer info
 * @retval  0 success, others failed
 */
RK_S32 RK_PLAYER_SetVideoSink(void *pPlayer,
                                  RK_PLAYER_FRAMEINFO_S *pstFrameInfo,
                                  void **ppSurface);

/**
 * @brief  do play of the stream
 * @param[in] pPlayer : RKADK_MW_PTR: handle of the player
 * @retval  0 success, others failed
 */
RK_S32 RK_PLAYER_Play(void *pPlayer);

/**
 * @brief stop the stream playing, and release the resource
 * @param[in] pPlayer : RKADK_MW_PTR: handle of the player
 * @param[in] pSurface : RKADK_MW_PTR: handle of the pSurface
 * @retval  0 success, others failed
 */
RK_S32 RK_PLAYER_Stop(void *pPlayer, void *pSurface);

/**
 * @brief pause the stream playing
 * @param[in] hPlayer : RKADK_MW_PTR: handle of the player
 * @retval  0 success, others failed
 */
RK_S32 RK_PLAYER_Pause(void *pPlayer);

/**
 * @brief seek by the time
 * @param[in] pPlayer : RKADK_MW_PTR: handle of the player
 * @param[in] s64TimeInMs : RKADK_S64: seek time
 * @retval  0 success, others failed
 */
RK_S32 RK_PLAYER_Seek(void *pPlayer, RK_S64 s64TimeInMs);

/**
 * @brief get the  current play status
 * @param[in] pPlayer : RKADK_MW_PTR: handle of the player
 * @param[out] penState : RKADK_LITEPLAYER_STATE_E*: play state
 *  @retval  0 success, others failed
 */
RK_S32 RK_PLAYER_GetPlayStatus(void *pPlayer,
                                  RK_PLAYER_STATE_E *penState);

#ifdef __cplusplus
}
#endif
#endif
