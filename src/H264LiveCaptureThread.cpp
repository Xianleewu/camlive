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

#include "fcntl.h"
#include "string.h"
#include <sys/time.h>

using namespace std;

static RkDrivingEncoder *rkh264_encoder_ = NULL;

void* doProc(void* arg)
{
    sp<ProcessState> proc(ProcessState::self());
    proc->startThreadPool();
    IPCThreadState::self()->joinThreadPool();
    pthread_exit((void*)"mService thread exited !\n");
}


H264LiveCaptureThread::H264LiveCaptureThread()
{	
}

H264LiveCaptureThread::~H264LiveCaptureThread()
{
	printf("obj deleted! \n");
	Destroy();
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

	if(!rkh264_encoder_)
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


void H264LiveCaptureThread::Destroy()
{
	printf("starting to detroy capture !\n");
//    if (mService)
//    {
//		mService->disconnect();
//		mService->removeClient();
//		//delete(mService);
//		mService = NULL;
//    }
//	
//	printf("mService destroied !\n");
//		
//	if(info.outputBuffer)
//	{
//		free(info.outputBuffer);
//		info.outputBuffer = NULL;
//	}

	printf("H264 capture exit !\n");

}

void H264LiveCaptureThread::Capture(void* obuf, int len, int* frameLen, int* truncatedLen)
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
		printf("######### no remotebuffer ready to draw...\n");
		//drawQueue.wait(drawlock);
        drawlock.unlock();
        return;
	}
    pbuf = drawWorkQ.itemAt(0);
    drawWorkQ.removeAt(0);
    drawlock.unlock();

    printf("draw preview Buffer: share_fd %d, index %d, w: %d h: %d , size: %d \n",
            pbuf->share_fd, pbuf->index, pbuf->width, pbuf->height, pbuf->size);

    unsigned char *vaddr = (unsigned char *)mmap(NULL, pbuf->size, PROT_READ | PROT_WRITE, MAP_SHARED, pbuf->share_fd, 0);
	//TODO:reconfig encoder and buffer when previewBuffer changed
	
	printf("mmap preview buffer suc ! \n");
		
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
	
	encodeProc(vaddr);

	if(info.mSize > len)
	{
		unexportLen =  info.mSize - len;
	}	

	printf("encode output size: %d \n",info.mSize);
		
	memcpy(obuf, info.outputBuffer, info.mSize - unexportLen);
	*truncatedLen = unexportLen;
	*frameLen = info.mSize - unexportLen;
	
	printf("export data suc! \n");	

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
	rkh264_encoder_ = new RkDrivingEncoder(pa);

	int ret = rkh264_encoder_->prepareEncode();
	if(ret != 0)
	{
		printf("prepare Encode failed \n");
       	return -1;
    }

    rkh264_encoder_->requestIDRFrame();
		
	printf("Init encoder suc ! \n");
	
	return 0;
}

void H264LiveCaptureThread::encodeProc(unsigned char *vddr)
{
	if(!rkh264_encoder_)
	{
		int ret = encoderInit();
		if(ret != 0)
		{
			printf("no avalvable encoder \n");
            return;
		}
	}
	
	//Input_Resource picture;
	memset(&picture, 0, sizeof(Input_Resource));
	printf("set input resource suc 1!\n");
    picture.mSize = preWidth * preHeight * 3 /2;//width * height * 3 / 2;
	printf("set input resource suc 2!\n");
    picture.inputBuffer = vddr;
    picture.mTimeUs = systemTime() / 1000;
	
	printf("set input resource suc 3!\n");
	
    //Output_Resource info;
 //   memset(&info, 0, sizeof(Output_Resource));
	printf("set input resource suc 3!\n");
    memset(info.outputBuffer, 0, preWidth * preHeight * 3 / 2);

    printf("Encoder frame  start \n");
    // Encode!
    int enc_ret = rkh264_encoder_->EncodeFrame(&picture, &info);

    printf("Encoder frame  end \n");

	//free(info.outputBuffer);

}


