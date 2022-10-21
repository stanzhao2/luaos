
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE     := libsocket.io
LOCAL_SRC_FILES  := ../../src/libsocket.io.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../../../include $(LOCAL_PATH)/../../../../3rd/asio

include $(BUILD_SHARED_LIBRARY)
