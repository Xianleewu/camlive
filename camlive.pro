TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    src/CameraServer.cpp \
    src/H264LiveCaptureThread.cpp \
    src/H264LiveFramedSource.cpp \
    src/H264LiveServerMediaSubsession.cpp \
    src/H264LiveCapture.c \
    src/webcam.c

HEADERS += \
    include/H264LiveCapture.h \
    include/H264LiveCaptureThread.hpp \
    include/H264LiveFramedSource.hpp \
    include/H264LiveServerMediaSubsession.hpp \
    include/webcam.h

INCLUDEPATH += \
    include \
    /usr/include \
    /usr/local/include \
    /usr/include/liveMedia \
    /usr/include/groupsock \
    /usr/include/BasicUsageEnvironment \
    /usr/include/UsageEnvironment

LIBS    += \
    -lx264 \
    -lavcodec \
    -lliveMedia \
    -lgroupsock \
    -lBasicUsageEnvironment \
    -lUsageEnvironment \
    -lavutil \
    -lavfilter \
    -lavdevice \
    -lavformat \
    -lswscale \
    -lswresample \
    -lpostproc \
    -lv4l2 \
    -lpthread \
    -lrt \
    -lm
