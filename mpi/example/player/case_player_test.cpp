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

#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "case_player_test.h"
#include "case_player_video.h"
#include "RTMediaPlayer.h"
#include "RTPlayerDef.h"
#include "rt_message.h"

extern int optind;
extern char *optarg;

static bool is_quit = false;
static char optstr[] = "i:x:y:hvg";
static RK_PLAYER_EVENT_FN g_pfnPlayerCallback = NULL;
static void *g_pPlayer = NULL;
static pthread_t g_thread = 0;
static bool g_getPosition = false;
static void *g_listener = NULL;
static int go_on = 0;

static void print_usage(const char *name) {
    printf("usage example:\n");
    printf("\t%s [-i xxx.wav] [-v] [-x 0] [-y 0]\n", name);
    printf("\t-i: input url, Default: /etc/bsa_file/8k8bpsMono.wav\n");
    printf("\t-v: video enable, Default: disable\n");
    printf("\t-x: display position X coordinate, Default: 0\n");
    printf("\t-y: display position y coordinate, Default: 0\n");
	printf("\t-g: go on play until exit\n");
    printf("\t-h: help\n");
}

static void sigterm_handler(int sig) {
    fprintf(stderr, "signal %d\n", sig);
    is_quit = true;
}

static void PlayerEventFnTest(void *pPlayer,
                                    RK_PLAYER_EVENT_E enEvent,
                                    void *pData) {
    int position = 0;

    switch (enEvent) {
    case RK_PLAYER_EVENT_STATE_CHANGED:
        printf("+++++ RK_PLAYER_EVENT_STATE_CHANGED +++++\n");
        break;
    case RK_PLAYER_EVENT_EOF:
        printf("+++++ RK_PLAYER_EVENT_EOF +++++\n");
        break;
    case RK_PLAYER_EVENT_SOF:
        printf("+++++ RK_PLAYER_EVENT_SOF +++++\n");
        break;
    case RK_PLAYER_EVENT_PROGRESS:
        if (pData)
            position = *(reinterpret_cast<int *>(pData));
        printf("+++++ RK_PLAYER_EVENT_PROGRESS(%d ms) +++++\n", position);
        break;
    case RK_PLAYER_EVENT_SEEK_END:
        printf("+++++ RK_PLAYER_EVENT_SEEK_END +++++\n");
        break;
    case RK_PLAYER_EVENT_ERROR:
        printf("+++++ RK_PLAYER_EVENT_ERROR +++++\n");
        break;
    case RK_PLAYER_EVENT_PREPARED:
        printf("+++++ RK_PLAYER_EVENT_PREPARED +++++\n");
        break;
    case RK_PLAYER_EVENT_STARTED:
        printf("+++++ RK_PLAYER_EVENT_STARTED +++++\n");
        break;
    case RK_PLAYER_EVENT_PAUSED:
        printf("+++++ RK_PLAYER_EVENT_PAUSED +++++\n");
        break;
    case RK_PLAYER_EVENT_STOPPED:
        printf("+++++ RK_PLAYER_EVENT_STOPPED +++++\n");
        break;
    default:
        printf("+++++ Unknown event(%d) +++++\n", enEvent);
        break;
    }
}

void *GetPlayPositionThread(void *para) {
    RK_S64 position;

    while (g_getPosition) {
        if (!g_pPlayer)
            break;

        (reinterpret_cast<RTMediaPlayer *>(g_pPlayer))->getCurrentPosition(&position);
        if (g_pfnPlayerCallback) {
            position = position / 1000;
            g_pfnPlayerCallback(g_pPlayer, RK_PLAYER_EVENT_PROGRESS,
                                reinterpret_cast<void *>(&position));
        }

        sleep(1);
    }

    RK_LOGD("Exit Thread");
    return NULL;
}

static int CreateGetPosThread() {
    if (!g_thread) {
        g_getPosition = true;
        if (pthread_create(&g_thread, NULL, GetPlayPositionThread, NULL)) {
            RK_LOGE("Create get play position thread failed");
            return -1;
        }
    }

    return 0;
}

static void DestoryGetPosThread() {
    int ret;

    g_getPosition = false;
    if (g_thread) {
        ret = pthread_join(g_thread, NULL);
        if (ret) {
            RK_LOGE("thread exit failed = %d", ret);
        } else {
            RK_LOGI("thread exit successed");
        }
        g_thread = 0;
    }
}

class RKPlayerListener : public RTPlayerListener {
 public:
    ~RKPlayerListener() { RK_LOGD("done"); }
    virtual void notify(INT32 msg, INT32 ext1, INT32 ext2, void *ptr) {
        RK_PLAYER_EVENT_E event = RK_PLAYER_EVENT_BUTT;

        RK_LOGI("msg: %d, ext1: %d", msg, ext1);
        switch (msg) {
        case RT_MEDIA_PREPARED:
            event = RK_PLAYER_EVENT_PREPARED;
            break;
        case RT_MEDIA_STARTED:
            event = RK_PLAYER_EVENT_STARTED;
            CreateGetPosThread();
            break;
        case RT_MEDIA_PAUSED:
            event = RK_PLAYER_EVENT_PAUSED;
            break;
        case RT_MEDIA_STOPPED:
            DestoryGetPosThread();
            event = RK_PLAYER_EVENT_STOPPED;
            break;
        case RT_MEDIA_PLAYBACK_COMPLETE:
            DestoryGetPosThread();
            event = RK_PLAYER_EVENT_EOF;
            break;
        case RT_MEDIA_BUFFERING_UPDATE:
            break;
        case RT_MEDIA_SEEK_COMPLETE:
            event = RK_PLAYER_EVENT_SEEK_END;
            break;
        case RT_MEDIA_ERROR:
            DestoryGetPosThread();
            event = RK_PLAYER_EVENT_ERROR;
            break;
        case RT_MEDIA_SET_VIDEO_SIZE:
        case RT_MEDIA_SKIPPED:
        case RT_MEDIA_TIMED_TEXT:
        case RT_MEDIA_INFO:
        case RT_MEDIA_SUBTITLE_DATA:
        case RT_MEDIA_SEEK_ASYNC:
            break;
        default:
            RK_LOGI("Unknown event: %d", msg);
        }

        if (event == RK_PLAYER_EVENT_BUTT)
            return;

        if (g_pfnPlayerCallback)
            g_pfnPlayerCallback(g_pPlayer, event, NULL);
    }
};

RK_S32 RK_PLAYER_Create(void **ppPlayer,
                        RK_PLAYER_CFG_S *pstPlayCfg) {
    int ret;

    RK_CHECK_POINTER(pstPlayCfg, RK_FAILURE);
    g_pfnPlayerCallback = pstPlayCfg->pfnPlayerCallback;

    RTMediaPlayer *mediaPlayer = new RTMediaPlayer();
    if (!mediaPlayer) {
        RK_LOGE("RTMediaPlayer failed");
        return -1;
    }

    *ppPlayer = reinterpret_cast<void *>(mediaPlayer);
    g_pPlayer = *ppPlayer;

    RKPlayerListener *listener = new RKPlayerListener();
    ret = mediaPlayer->setListener(listener);
    if (ret) {
        RK_LOGE("setListener failed = %d", ret);
        return ret;
    }
    g_listener = listener;

    return 0;
}

RK_S32 RK_PLAYER_Destroy(void *pPlayer, void *pSurface) {
    int ret;

    RK_CHECK_POINTER(pPlayer, RK_FAILURE);
    RK_CHECK_POINTER(pSurface, RK_FAILURE);

    ret = (reinterpret_cast<RTMediaPlayer *>(pPlayer))->stop();
    if (ret) {
        RK_LOGE("RTMediaPlayer stop failed(%d)", ret);
        return ret;
    }

    (reinterpret_cast<RKVideoPlayer*>(pSurface))->replay();
    (reinterpret_cast<RTMediaPlayer *>(pPlayer))->reset();
    delete (reinterpret_cast<RTMediaPlayer *>(pPlayer));
    g_pPlayer = NULL;

    delete (reinterpret_cast<RKPlayerListener *>(g_listener));
    g_listener = NULL;

    return 0;
}

RK_S32 RK_PLAYER_SetDataSource(void *pPlayer,
                                  const char *pszfilePath) {
    RK_CHECK_POINTER(pPlayer, RK_FAILURE);
    RK_CHECK_POINTER(pszfilePath, RK_FAILURE);

    return (reinterpret_cast<RTMediaPlayer *>(pPlayer))->setDataSource(pszfilePath, NULL);
}

RK_S32 RK_PLAYER_Prepare(void *pPlayer) {
    RK_CHECK_POINTER(pPlayer, RK_FAILURE);

    return (reinterpret_cast<RTMediaPlayer *>(pPlayer))->prepare();
}

RK_S32 RK_PLAYER_SetVideoSink(void *pPlayer,
                              RK_PLAYER_FRAMEINFO_S *pstFrameInfo,
                              void **ppSurface) {
    RK_CHECK_POINTER(pPlayer, RK_FAILURE);
    RK_CHECK_POINTER(pstFrameInfo, RK_FAILURE);
    RK_CHECK_POINTER(ppSurface, RK_FAILURE);

    RKVideoPlayer *pSurface = new RKVideoPlayer();
    if (!pSurface) {
        RK_LOGE("RKVideoPlayer failed");
        return -1;
    }
    *ppSurface = reinterpret_cast<void *>(pSurface);

    pSurface->pstFrmInfo = pstFrameInfo;

    return (reinterpret_cast<RTMediaPlayer *>(pPlayer))
        ->setVideoSink(static_cast<const void *>(pSurface));
}

RK_S32 RK_PLAYER_Play(void *pPlayer) {
    RK_CHECK_POINTER(pPlayer, RK_FAILURE);

    return (reinterpret_cast<RTMediaPlayer *>(pPlayer))->start();
}

RK_S32 RK_PLAYER_Stop(void *pPlayer, void *pSurface) {
    int ret;

    RK_CHECK_POINTER(pPlayer, RK_FAILURE);

    ret = (reinterpret_cast<RTMediaPlayer *>(pPlayer))->stop();
    if (ret) {
        RK_LOGE("RTMediaPlayer stop failed(%d)", ret);
        return ret;
    }
    (reinterpret_cast<RKVideoPlayer*>(pSurface))->replay();
    ret = (reinterpret_cast<RTMediaPlayer *>(pPlayer))->reset();

    return ret;
}

RK_S32 RK_PLAYER_Pause(void *pPlayer) {
    RK_CHECK_POINTER(pPlayer, RK_FAILURE);

    return (reinterpret_cast<RTMediaPlayer *>(pPlayer))->pause();
}

RK_S32 RK_PLAYER_Seek(void *pPlayer, RK_S64 s64TimeInMs) {
    RK_CHECK_POINTER(pPlayer, RK_FAILURE);

    return (reinterpret_cast<RTMediaPlayer *>(pPlayer))->seekTo(s64TimeInMs * 1000);
}

RK_S32 RK_PLAYER_GetPlayStatus(void *pPlayer,
                                RK_PLAYER_STATE_E *penState) {
    RK_U32 state;
    RK_PLAYER_STATE_E enState = RK_PLAYER_STATE_BUTT;

    RK_CHECK_POINTER(pPlayer, RK_FAILURE);
    RK_CHECK_POINTER(penState, RK_FAILURE);

    state = (reinterpret_cast<RTMediaPlayer *>(pPlayer))->getState();
    switch (state) {
    case RT_STATE_IDLE:
        enState = RK_PLAYER_STATE_IDLE;
        break;
    case RT_STATE_INITIALIZED:
        enState = RK_PLAYER_STATE_INIT;
        break;
    case RT_STATE_PREPARED:
        enState = RK_PLAYER_STATE_PREPARED;
        break;
    case RT_STATE_STARTED:
        enState = RK_PLAYER_STATE_PLAY;
        break;
    case RT_STATE_PAUSED:
        enState = RK_PLAYER_STATE_PAUSE;
        break;
    case RT_STATE_ERROR:
        enState = RK_PLAYER_STATE_ERR;
        break;
    case RT_STATE_PREPARING:
    case RT_STATE_STOPPED:
    case RT_STATE_COMPLETE:
        break;
    default:
        RK_LOGE("UnKnown play state = %d", state);
        break;
    }

    *penState = enState;
    return 0;
}

void param_init(RK_PLAYER_FRAMEINFO_S *pstFrmInfo) {
    RK_CHECK_POINTER_N(pstFrmInfo);

    pstFrmInfo->u32FrmInfoS32x = 0;
    pstFrmInfo->u32FrmInfoS32y = 0;
    pstFrmInfo->u32DispWidth = 0;
    pstFrmInfo->u32DispHeight = 0;
    pstFrmInfo->u32ImgWidth = 0;
    pstFrmInfo->u32ImgHeight = 0;
    pstFrmInfo->u32VoLayerMode = 2;
    pstFrmInfo->u32VoFormat = VO_FORMAT_NV12;
    pstFrmInfo->u32VoDev = VO_DEV_HD0;
    pstFrmInfo->u32EnIntfType = DISPLAY_TYPE_MIPI;
    pstFrmInfo->u32DispFrmRt = 30;
    pstFrmInfo->enIntfSync = VO_OUTPUT_DEFAULT;
    pstFrmInfo->u32EnMode = CHNN_ASPECT_RATIO_AUTO;
    pstFrmInfo->u32BorderTopWidth = 0;
    pstFrmInfo->u32BorderBottomWidth = 0;
    pstFrmInfo->u32BorderLeftWidth = 0;
    pstFrmInfo->u32BorderRightWidth = 0;
    pstFrmInfo->u32BorderColor = 0x0000FA;
    pstFrmInfo->u32ChnnNum = 1;
    pstFrmInfo->stSyncInfo.bIdv = RK_TRUE;
    pstFrmInfo->stSyncInfo.bIhs = RK_TRUE;
    pstFrmInfo->stSyncInfo.bIvs = RK_TRUE;
    pstFrmInfo->stSyncInfo.bSynm = RK_TRUE;
    pstFrmInfo->stSyncInfo.bIop = RK_TRUE;
    pstFrmInfo->stSyncInfo.u16FrameRate = 50;
    pstFrmInfo->stSyncInfo.u16PixClock = 65000;
    pstFrmInfo->stSyncInfo.u16Hact = 1200;
    pstFrmInfo->stSyncInfo.u16Hbb = 24;
    pstFrmInfo->stSyncInfo.u16Hfb = 240;
    pstFrmInfo->stSyncInfo.u16Hpw = 136;
    pstFrmInfo->stSyncInfo.u16Hmid = 0;
    pstFrmInfo->stSyncInfo.u16Vact = 1200;
    pstFrmInfo->stSyncInfo.u16Vbb = 200;
    pstFrmInfo->stSyncInfo.u16Vfb = 194;
    pstFrmInfo->stSyncInfo.u16Vpw = 6;
    return;
}

int main(int argc, char *argv[]) {
    RK_PLAYER_FRAMEINFO_S stFrmInfo;
    int c, ret;
    const char *file = "/data/tt.mp4";
    void *pPlayer;
    void *pSurface;
    RK_BOOL bVideoEnable = RK_TRUE;

    param_init(&stFrmInfo);

    while ((c = getopt(argc, argv, optstr)) != -1) {
        switch (c) {
        case 'i':
            file = optarg;
            break;
        case 'v':
            bVideoEnable = RK_TRUE;
            break;
        case 'x':
            stFrmInfo.u32FrmInfoS32x = atoi(optarg);
            break;
        case 'y':
            stFrmInfo.u32FrmInfoS32y = atoi(optarg);
            break;
		case 'g':
			go_on = 1;
			break;
        case 'h':
        default:
            print_usage(argv[0]);
            optind = 0;
            return 0;
        }
    }
    optind = 0;

    RK_LOGD("#play file: %s", file);

    signal(SIGINT, sigterm_handler);

    RK_PLAYER_CFG_S stPlayCfg;
    stPlayCfg.bEnableAudio = RK_TRUE;
    if (bVideoEnable)
        stPlayCfg.bEnableVideo = RK_TRUE;

    stPlayCfg.pfnPlayerCallback = PlayerEventFnTest;
    if (RK_PLAYER_Create(&pPlayer, &stPlayCfg)) {
        RK_LOGE("RK_PLAYER_Create failed");
        return -1;
    }

    RK_PLAYER_SetDataSource(pPlayer, file);

    if (bVideoEnable)
        RK_PLAYER_SetVideoSink(pPlayer, &stFrmInfo, &pSurface);

    RK_PLAYER_Prepare(pPlayer);

    RK_PLAYER_Play(pPlayer);
    // RK_PLAYER_Seek(pPlayer, 1000); //seek 1s

    char cmd[64];
    printf("\n#Usage: input 'quit' to exit programe!\n"
          "peress any other key to capture one picture to file\n");
    while (!is_quit) {
		if(!go_on) {
			fgets(cmd, sizeof(cmd), stdin);
			RK_LOGD("#Input cmd: %s", cmd);
			if (strstr(cmd, "quit") || is_quit) {
				RK_LOGD("#Get 'quit' cmd!");
				break;
			}
		}else
			usleep(5000 * 1000);

        RK_PLAYER_Stop(pPlayer, pSurface);
        ret = RK_PLAYER_SetDataSource(pPlayer, file);
        if (ret) {
            RK_LOGE("SetDataSource failed, ret = %d", ret);
            break;
        }

        ret = RK_PLAYER_Prepare(pPlayer);
        if (ret) {
            RK_LOGE("Prepare failed, ret = %d", ret);
            break;
        }

        ret = RK_PLAYER_Play(pPlayer);
        if (ret) {
            RK_LOGE("Play failed, ret = %d", ret);
            break;
        }

        usleep(500 * 1000);
    }

    RK_PLAYER_Destroy(pPlayer, pSurface);
    return 0;
}
