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

#include "H264LiveCaptureThread.hpp"


void* doProc(void* arg)
{
    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();

    while(running)
    {
        sleep(1);
    }

    pthread_exit((void*)"mService thread exited !\n");
}


H264LiveCaptureThread::H264LiveCaptureThread()
{
    rkH264Encoder = NULL;
    mService = NULL;
    info.outputBuffer = NULL;
}

bool H264LiveCaptureThread::Create(int width, int height, int fps)
{
    printf("starting to creat capture thread !\n");

    preWidth = 640;
    preHeight = 360;

    if(mService != NULL)
    {
        printf("mService created in another time! \n");
        return false;
    }

    size = 32;
    pbuf = new RemotePreviewBuffer;
    buf = (unsigned char *)pbuf;

    pbuf->index = 0x55;
    pbuf->size = sizeof(RemotePreviewBuffer);
    pbuf->share_fd = 0xaa;

    mService = new RemoteBufferWrapper();

    if(mService == NULL)
    {
        printf("Error when creat a service \n");
        return false;
    }

    if(!rkH264Encoder)
    {
        encoderInit();
    }

    if(!info.outputBuffer)
    {
        info.outputBuffer = malloc(preWidth * preHeight * 4 / 2);
    }
    mService->connect();
    mService->setPreviewCallback(this);
    mService->call(buf, size);
    mService->startPreview(0);

    int ret = pthread_create(&procTh,NULL,doProc,NULL);

    if(ret != 0)
    {
        printf("creating new thread error !\n");
        return -1;
    }

    printf("Service started !\n");

    return true;
}

H264LiveCaptureThread::~H264LiveCaptureThread()
{
    printf("obj deleted! \n");
    Destroy();
}

void H264LiveCaptureThread::Destroy()
{
    printf("starting to detroy capture !\n");

    running = false;

    while(drawWorkQ.size() > 0)
    {
        drawlock.lock();
        RemotePreviewBuffer *pbuf2;
        pbuf2 = drawWorkQ.itemAt(0);
        drawWorkQ.removeAt(0);
        drawlock.unlock();

        mService->releasePreviewFrame((unsigned char *)pbuf2, sizeof(RemotePreviewBuffer));
        close(pbuf2->share_fd);
        delete pbuf2;
    }


    if (mService != NULL)
    {
        mService->disconnect();
        mService->removeClient();
        mService = NULL;
    }

    //	printf("mService destroied !\n");
    //
    //	if(info.outputBuffer)
    //	{
    //		free(info.outputBuffer);
    //		info.outputBuffer = NULL;
    //	}

    printf("H264 capture exit !\n");

}

void H264LiveCaptureThread::exportData(void* obuf, int len, int* frameLen, int* truncatedLen)
{
    int unexportLen = 0;

    *truncatedLen = 0;
    *frameLen = 0;

    if (mService == NULL)
    {
        printf("Service is not avaleable ! \n");
        return;
    }

    RemotePreviewBuffer *pbuf;
    drawlock.lock();
    if(drawWorkQ.isEmpty())
    {
        printf("no remotebuffer ready to draw...\n");
        //drawQueue.wait(drawlock);
        drawlock.unlock();
        return;
    }
    pbuf = drawWorkQ.itemAt(0);
    drawWorkQ.removeAt(0);
    drawlock.unlock();

    unsigned char *vaddr = (unsigned char *)mmap(NULL, pbuf->size, PROT_READ | PROT_WRITE, MAP_SHARED, pbuf->share_fd, 0);

    //reconfig encoder and buffer when previewBuffer changed
    if(pbuf->width != preWidth || pbuf->height != preHeight)
    {
        preWidth = pbuf->width;
        preHeight = pbuf->height;

        encoderInit();
        if(info.outputBuffer)
        {
            free(info.outputBuffer);
        }
        info.outputBuffer = malloc(preWidth * preHeight * 3 / 2);
    }

    if(encodeProc(vaddr) < 0)
    {
        goto cleanUp;
        return;
    }

    if(info.mSize > len)
    {
        unexportLen =  info.mSize - len;
    }

    memcpy(obuf, info.outputBuffer, info.mSize - unexportLen);
    *truncatedLen = unexportLen;
    *frameLen = info.mSize - unexportLen;

cleanUp:
    munmap(vaddr, pbuf->size);
    mService->releasePreviewFrame((unsigned char *)pbuf, sizeof(RemotePreviewBuffer));
    close(pbuf->share_fd);
    delete pbuf;
}

int H264LiveCaptureThread::encoderInit()
{
    InitParams_t param;
    InitParams_t *pa = &param;
    pa->mVideoWidth = preWidth; // FIXME:only for debug
    pa->mVideoHeight = preHeight; //FIXME:
    pa->mVideoFrameRate = 25;
    pa->mVideoBitRate = 2*1024*1024;
    pa->mVideoColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    rkH264Encoder = new RkDrivingEncoder(pa);

    int ret = rkH264Encoder->prepareEncode();
    if(ret != 0)
    {
        printf("prepare Encode failed \n");
        return -1;
    }

    rkH264Encoder->requestIDRFrame();

    return 0;
}

int H264LiveCaptureThread::encodeProc(unsigned char *vddr)
{
    if(!rkH264Encoder)
    {
        int ret = encoderInit();
        if(ret != 0)
        {
            printf("no avalvable encoder \n");
            return -1;
        }
    }

    //Input_Resource picture;
    memset(&picture, 0, sizeof(Input_Resource));
    picture.mSize = preWidth * preHeight * 3 /2;
    picture.inputBuffer = vddr;
    picture.mTimeUs = systemTime() / 1000;

    //Output_Resource info;
    //memset(&info, 0, sizeof(Output_Resource));
    memset(info.outputBuffer, 0, preWidth * preHeight * 3 / 2);

    // Encode!
    int ret = rkH264Encoder->EncodeFrame(&picture, &info);

    return ret;
}


