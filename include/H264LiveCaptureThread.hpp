/*
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#ifndef _H264_LIVE_CAPTURE_THREAD_HPP
#define _H264_LIVE_CAPTURE_THREAD_HPP

#define LOG_TAG "RemoteClient"
#include <stdio.h>
#include <ProcessState.h>
#include <IPCThreadState.h>
#include <utils/Vector.h>
#include <sys/mman.h>

#include <OMX_Video.h>
#include <utils/Log.h>
#include <binder/IServiceManager.h>
#include <RkDrivingEncoder.h>
#include "RemoteBuffer.h"
using namespace rockchip;

#define H264_MAX_FRAME_SIZE (100 * 1024)
static Vector<RemotePreviewBuffer *> drawWorkQ;
static bool running = true;

class H264LiveCaptureThread : public RemoteBufferWrapper::RemotePreviewCallback
{
public:
    H264LiveCaptureThread();
    ~H264LiveCaptureThread();

    RemoteBufferWrapper *mService;

    bool Create(int width, int height, int fps);

    void Destroy();

    void exportData(void* buf, int len, int* frameLen, int* truncatedLen);

    virtual void onCall(unsigned char *pbuf, int size)
    {
        printf("onCall \n");
    }

    virtual void onPreviewData(int shared_fd, unsigned char *pbuf, int size)
    {
        printf("got previewFrame \n");

        RemotePreviewBuffer *pbuf1 = new RemotePreviewBuffer;
        RemotePreviewBuffer *data = (RemotePreviewBuffer *)pbuf;

        pbuf1->share_fd = dup(shared_fd);
        pbuf1->index = data->index;
        pbuf1->width = data->width;
        pbuf1->height = data->height;
        pbuf1->stride = data->stride;
        pbuf1->size = data->size;
        drawlock.lock();
        drawWorkQ.push_back(pbuf1);
        drawQueue.broadcast();
        drawlock.unlock();

    }

private:
    int preWidth;
    int preHeight;

    int size;
    RemotePreviewBuffer *pbuf;
    unsigned char *buf;

    Mutex drawlock;
    Condition drawQueue;

    char mFrameBuf[H264_MAX_FRAME_SIZE];
    Input_Resource picture;
    Output_Resource info;
    pthread_t procTh;

    RkDrivingEncoder *rkH264Encoder;
    int encoderInit();
    int encodeProc(unsigned char* vddr);
};

#endif
