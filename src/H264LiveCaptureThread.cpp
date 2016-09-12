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


H264LiveCaptureThread::H264LiveCaptureThread()
{
    running = true;
    preWidth = 640;
    preHeight = 360;
    preBitRate = 4 * 1024 * 1024;

    rkH264Encoder = NULL;
    outPicture.outputBuffer = NULL;
}

bool H264LiveCaptureThread::Create(unsigned char camid, int bitrate)
{
    printf("starting to creat capture thread !\n");

    preBitRate = bitrate;

    size = 32;
    pbuf = new RemotePreviewBuffer;
    buf = (unsigned char *)pbuf;

    pbuf->index = 0x55;
    pbuf->size = sizeof(RemotePreviewBuffer);
    pbuf->share_fd = 0xaa;

    if(!rkH264Encoder)
    {
        encoderInit();
    }

    if(!outPicture.outputBuffer)
    {
        outPicture.outputBuffer = malloc(preWidth * preHeight * 3 / 2);
    }

    mService.connect();
    mService.setPreviewCallback(this);
    mService.call(buf, size);
    mService.startPreview(0);

    int ret = pthread_create(&procTh,NULL,H264LiveCaptureThread::doProc,this);
    //ret = pthread_create(&releTh,NULL,H264LiveCaptureThread::doRelease,this);

    if(ret != 0)
    {
        printf("creating new thread error !\n");
        return -1;
    }
    printf("Service started !\n");

    return true;
}

void* H264LiveCaptureThread::doProc(void* arg)
{
    H264LiveCaptureThread *nThread = (H264LiveCaptureThread*)arg;
    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();

    while(nThread->running)
    {
        sleep(1);
    }

    pthread_exit((void*)"mService thread exited !\n");
}

void* H264LiveCaptureThread::doRelease(void* arg)
{
    H264LiveCaptureThread* nThread = (H264LiveCaptureThread*)arg;
    while(nThread->running)
    {

        if(nThread->drawWorkQ.size() > 1)
        {
            nThread->drawlock.lock();
            RemotePreviewBuffer *pbuf2;
            pbuf2 = nThread->drawWorkQ.itemAt(0);
            nThread->drawWorkQ.removeAt(0);
            nThread->drawlock.unlock();

            nThread->mService.releasePreviewFrame((unsigned char *)pbuf2, sizeof(RemotePreviewBuffer));
            close(pbuf2->share_fd);
            delete pbuf2;
        }


    }

    pthread_exit((void*)"mService thread exited !\n");
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

    pthread_join(procTh, NULL);

    drawlock.lock();
    while(drawWorkQ.size() > 0)
    {
        RemotePreviewBuffer *pbuf2;
        pbuf2 = drawWorkQ.itemAt(0);
        drawWorkQ.removeAt(0);
        drawlock.unlock();

        mService.releasePreviewFrame((unsigned char *)pbuf2, sizeof(RemotePreviewBuffer));
        close(pbuf2->share_fd);
        delete pbuf2;
    }


    mService.disconnect();
    mService.removeClient();

    if(outPicture.outputBuffer)
    {
        free(outPicture.outputBuffer);
    }

    printf("H264 capture exit !\n");

}

void H264LiveCaptureThread::exportData(void* obuf, int len, int* frameLen, int* truncatedLen)
{
    int unexportLen = 0;

    *truncatedLen = 0;
    *frameLen = 0;

#ifdef EXPORT_TIME
    timeval sTime;
    timeval eTime;
    float time_use = 0;

    gettimeofday(&sTime,NULL);
#endif

    RemotePreviewBuffer *pbuf;
    //drawlock.lock();
    if(drawWorkQ.isEmpty())
    {
        printf("no remotebuffer ready to draw...\n");
        //drawlock.unlock();
        return;
    }
    pbuf = drawWorkQ.itemAt(0);
    drawWorkQ.removeAt(0);
    //drawlock.unlock();

    unsigned char *vaddr = (unsigned char *)mmap(NULL, pbuf->size, PROT_READ | PROT_WRITE, MAP_SHARED, pbuf->share_fd, 0);

    //reconfig encoder and buffer when previewBuffer changed
    if(pbuf->width != preWidth || pbuf->height != preHeight)
    {
        preWidth = pbuf->width;
        preHeight = pbuf->height;

        encoderInit();
        if(outPicture.outputBuffer)
        {
            free(outPicture.outputBuffer);
        }
        outPicture.outputBuffer = malloc(preWidth * preHeight * 3 / 2);
    }

    if(encodeProc(vaddr) < 0)
    {
        goto cleanUp;
        return;
    }

    if(outPicture.mSize > len)
    {
        unexportLen =  outPicture.mSize - len;
    }

    memcpy(obuf, outPicture.outputBuffer, outPicture.mSize - unexportLen);
    *truncatedLen = unexportLen;
    *frameLen = outPicture.mSize - unexportLen;

#ifdef EXPORT_TIME
    gettimeofday(&eTime,NULL);
    time_use=(eTime.tv_sec-sTime.tv_sec)*1000000+(eTime.tv_usec-sTime.tv_usec);
    printf("export using time %f us \n",time_use);
#endif

cleanUp:
    munmap(vaddr, pbuf->size);
    mService.releasePreviewFrame((unsigned char *)pbuf, sizeof(RemotePreviewBuffer));
    close(pbuf->share_fd);
    delete pbuf;
}

int H264LiveCaptureThread::encoderInit()
{
    InitParams_t param;
    InitParams_t *pa = &param;
    pa->mVideoWidth = preWidth;
    pa->mVideoHeight = preHeight;
    pa->mVideoFrameRate = 15;
    pa->mVideoBitRate = preBitRate;
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

    //Input_Resource inPicture;
    memset(&inPicture, 0, sizeof(Input_Resource));
    inPicture.mSize = preWidth * preHeight * 3 /2;
    inPicture.inputBuffer = vddr;
    inPicture.mTimeUs = systemTime() / 1000;

    //Output_Resource outPicture;
    //memset(&outPicture, 0, sizeof(Output_Resource));
    memset(outPicture.outputBuffer, 0, preWidth * preHeight * 3 / 2);

    // Encode!
    int ret = rkH264Encoder->EncodeFrame(&inPicture, &outPicture);

    return ret;
}


