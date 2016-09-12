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

#include <stdio.h>
#include <sys/mman.h>
#include <sys/time.h>

#include <ProcessState.h>
#include <IPCThreadState.h>
#include <utils/Vector.h>
#include <OMX_Video.h>
#include <utils/Log.h>
#include <binder/IServiceManager.h>
#include <RkDrivingEncoder.h>
#include "RemoteBuffer.h"
using namespace rockchip;

#define H264_MAX_FRAME_SIZE (100 * 1024)

//#define GRABBER_TIME
//#define EXPORT_TIME

typedef struct _preViewBuffer
{
    char *vAddr;
    unsigned int vSize;
}preViewBuffer;

class H264LiveCaptureThread : public RemoteBufferWrapper::RemotePreviewCallback
{
public:
    H264LiveCaptureThread();
    ~H264LiveCaptureThread();

    bool Create(unsigned char camid, int bitrate);

    void Destroy();

    void exportData(void* buf, int len, int* frameLen, int* truncatedLen);

    virtual void onCall(unsigned char *pbuf, int size)
    {

    }

    virtual void onPreviewData(int shared_fd, unsigned char *pbuf, int size)
    {
        //printf("got previewFrame \n");

#ifdef GRABBER_TIME
        timeval sTime;
        timeval eTime;
        float time_use = 0;

        gettimeofday(&sTime,NULL);
#endif
        RemotePreviewBuffer *nRePreBuf = new RemotePreviewBuffer;
        RemotePreviewBuffer *data = (RemotePreviewBuffer *)pbuf;

        nRePreBuf->share_fd = dup(shared_fd);
        nRePreBuf->index = data->index;
        nRePreBuf->width = data->width;
        nRePreBuf->height = data->height;
        nRePreBuf->stride = data->stride;
        nRePreBuf->size = data->size;

        if(drawWorkQ.isEmpty())
        {
            drawlock.lock();
            drawWorkQ.push_back(nRePreBuf);
            drawlock.unlock();
        }
        else
        {
            drawWorkQ.push_back(nRePreBuf);

            if(drawWorkQ.size() > 2)
            {
                RemotePreviewBuffer *pbuf2;
                pbuf2 = drawWorkQ.itemAt(1);
                drawWorkQ.removeAt(1);

                mService.releasePreviewFrame((unsigned char *)pbuf2, sizeof(RemotePreviewBuffer));
                close(pbuf2->share_fd);
                delete pbuf2;
            }

        }

#ifdef GRABBER_TIME
        gettimeofday(&eTime,NULL);
        time_use=(eTime.tv_sec-sTime.tv_sec)*1000000+(eTime.tv_usec-sTime.tv_usec);
        printf("grabber using time %f us \n",time_use);
#endif
    }

private:
    int preBitRate;
    int preWidth;
    int preHeight;
    bool running;

    Mutex drawlock;
    Vector<RemotePreviewBuffer *> drawWorkQ;
    RemoteBufferWrapper mService;

    int size;
    RemotePreviewBuffer *pbuf;
    unsigned char *buf;

    Input_Resource inPicture;
    Output_Resource outPicture;
    pthread_t procTh;
    pthread_t releTh;

    static void* doProc(void* arg);
    static void* doRelease(void* arg);

    RkDrivingEncoder *rkH264Encoder;
    int encoderInit();
    int encodeProc(unsigned char* vddr);
};

#endif
