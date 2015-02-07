ifeq ($(BOARD_HAVE_BLUETOOTH_BCM),true)

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= brcm_bt_helper.c
LOCAL_MODULE := brcm_bt_helper
LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_MODULE_TAGS := eng 
include $(BUILD_EXECUTABLE)

endif

