#
# libbluedroid
#

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	bluetooth.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	system/bluetooth/bluez-clean-headers

ifeq ($(BT_ALT_STACK),true)
LOCAL_CFLAGS += -DBT_ALT_STACK
endif

LOCAL_SHARED_LIBRARIES := \
	libcutils

LOCAL_MODULE := libbluedroid

include $(BUILD_SHARED_LIBRARY)
