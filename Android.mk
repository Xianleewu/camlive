LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

PRE_BUILD_STDCXX_PATH := prebuilts/ndk/9/sources/cxx-stl/gnu-libstdc++/4.8/libs/x86

LOCAL_SRC_FILES := \
	src/CameraServer.cpp \
	src/H264LiveCaptureThread.cpp \
	src/H264LiveFramedSource.cpp \
	src/H264LiveServerMediaSubsession.cpp

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	external/live/UsageEnvironment/include \
    external/live/groupsock/include \
    external/live/BasicUsageEnvironment/include \
    external/live/liveMedia/include \
	bionic \
    system/core/include \
    frameworks/native/include/binder \
	frameworks/av/media_proxy/RkDrivingEncoder \
	frameworks/native/include/media/openmax \
	hardware/rockchip/librkvpu \
    app/newcdr/src/remote \
    app/newcdr/include/display  \
    app/newcdr/include/fwk_msg

LOCAL_STATIC_LIBRARIES := \
	libliveMedia \
	libBasicUsageEnvironment \
	libUsageEnvironment \
	libgroupsock

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libbinder \
    libfwk_msg \
    libremotebuffer \
	libmedia_proxy \
	libmedia \
	libion \
	librkdrivingenc

LOCAL_LDLIBS := -ldl

LOCAL_LDFLAGS := -L$(PRE_BUILD_STDCXX_PATH) -lgnustl_static -lsupc++

LOCAL_CFLAGS := -Wno-error=non-virtual-dtor

LOCAL_CPPFALGS := -fexceptions

LOCAL_MODULE := camlive

LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
