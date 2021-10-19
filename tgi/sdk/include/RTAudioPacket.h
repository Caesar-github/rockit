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

#ifndef SRC_RT_MEDIA_INCLUDE_RTASTREAMBUFFER_H_
#define SRC_RT_MEDIA_INCLUDE_RTASTREAMBUFFER_H_

#include "RTAVShellBuffer.h"


class RTAudioPacket : public RTAVShellBuffer {
 public:
    RTAudioPacket();
    virtual ~RTAudioPacket() = default;

    virtual RTMediaBufferType getType();

    UINT64    getPts() const;
    UINT64    getDts() const;
    INT32     getDuration() const;
    UINT32    getSeq() const;

    void  setPts(const UINT64 pts);
    void  setDts(const UINT64 dts);
    void  setDuration(const INT32 duration);
    void  setSeq(const UINT32 seq);

 protected:
    virtual void reset();

 private:
    void baseInit();

 private:
     UINT64  mPts;
     UINT64  mDts;
     UINT32  mSeq;
     INT32   mDuration;
};

RTAudioPacket* reinterpret_apacket(RTMediaBuffer *buffer);
RTAudioPacket* construct_apacket(RTMediaBuffer *buffer);

#endif  // SRC_RT_MEDIA_INCLUDE_RTASTREAMBUFFER_H_
