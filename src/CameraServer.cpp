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

#include "H264LiveServerMediaSubsession.hpp"
#include <BasicUsageEnvironment.hh>
#include <liveMedia.hh>
#include <string.h>

#define CAMERA_SERVER_VERSION_STRING        "0.1"

#define VIDEO_WIDTH     640
#define VIDEO_HEIGTH    480
#define FRAME_PER_SEC   25.0

int main(int argc, char** argv)
{
    char* videoDevice =(char*)"/dev/video1";
	
	if(argc > 1)
	{
		videoDevice = argv[1];
	}
	
    // Begin by setting up our usage environment:
    TaskScheduler* scheduler = BasicTaskScheduler::createNew();
    UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

    UserAuthenticationDatabase* authDB = NULL;
#ifdef ACCESS_CONTROL
    // To implement client access control to the RTSP server, do the following:
    authDB = new UserAuthenticationDatabase;
    authDB->addUserRecord("username1", "password1"); // replace these with real strings
    // Repeat the above with each <username>, <password> that you wish to allow
    // access to the server.
#endif

    // Create the RTSP server.  Try first with the default port number (554),
    // and then with the alternative port number (8554):
    RTSPServer* rtspServer;
    portNumBits rtspServerPortNum = 554;
    rtspServer = RTSPServer::createNew(*env, rtspServerPortNum, authDB);
    if (rtspServer == NULL) 
    {
        rtspServerPortNum = 8554;
        rtspServer = RTSPServer::createNew(*env, rtspServerPortNum, authDB);
    }
    if (rtspServer == NULL) 
    {
        *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
        exit(1);
    }

    *env << "Camera server version " << CAMERA_SERVER_VERSION_STRING << "\n";

    ServerMediaSession *sms = ServerMediaSession::createNew(*env, 
        "webcam", 0, "Camera server, streamed by Xianleewu");   
    sms->addSubsession(H264LiveServerMediaSubsession::createNew(*env, 
        videoDevice, VIDEO_WIDTH, VIDEO_HEIGTH, FRAME_PER_SEC)); 
    rtspServer->addServerMediaSession(sms);  

    char* url = rtspServer->rtspURL(sms);  
    *env << "Using url \"" << url << "\"\n";  
    delete[] url; 

    env->taskScheduler().doEventLoop(); // does not return

    return 0; // only to prevent compiler warning
}
