#
# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
ifeq ($(BOARD_HAVE_BLUETOOTH),true)

LOCAL_PATH := $(call my-dir)


# copy brcm binaries into android
#
PRODUCT_COPY_FILES += \
 	$(LOCAL_PATH)/bin/btld_stripped:system/bin/btld \
	$(LOCAL_PATH)/bin/BCM4329B1_002_1_002_023_0797_0834.hcd:system/bin/BCM4329B1_002_1_002_023_0797_0834.hcd

include $(call all-named-subdir-makefiles, adaptation/btl-if/client)

else
$(info ************************************************************)
$(info this makefile requires BOARD_HAVE_BLUETOOTH to be defined to true, or remove it)
$(info ************************************************************)
endif
