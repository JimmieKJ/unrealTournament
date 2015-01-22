#
# import_vrlib.mk
#
# Common settings used by all VRLib based projects.  Import at the stat of any module using VRLib.
#
# use += instead of := when defining the following variables after including this file: 
# 	LOCAL_LDLIBS
# 	LOCAL_CFLAGS
# 	LOCAL_C_INCLUDES
# 	LOCAL_STATIC_LIBRARIES 
#

# save off the local path
LOCAL_PATH_TEMP := $(LOCAL_PATH)
LOCAL_PATH := $(call my-dir)

# oculus
include $(CLEAR_VARS)

LOCAL_MODULE := oculus

LOCAL_SRC_FILES := obj/local/armeabi-v7a/liboculus.a

include $(PREBUILT_STATIC_LIBRARY)

# jpeg
include $(CLEAR_VARS)

LOCAL_MODULE := jpeg

LOCAL_SRC_FILES := jni/3rdParty/libjpeg.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_LDLIBS	+= -lGLESv3			# OpenGL ES 3.0
LOCAL_LDLIBS	+= -lEGL			# GL platform interface
LOCAL_LDLIBS    += -lOpenMAXAL		# native multimedia
LOCAL_LDLIBS    += -llog			# logging
LOCAL_LDLIBS    += -landroid		# native windows
LOCAL_LDLIBS    += -lz				# For minizip
LOCAL_LDLIBS    += -lOpenSLES		# audio

# native activities need this, regular java projects don't
# LOCAL_STATIC_LIBRARIES := android_native_app_glue

LOCAL_C_INCLUDES := $(LOCAL_PATH)/jni/LibOVR/Include \
                    $(LOCAL_PATH)/jni/LibOVR/Src \
                    $(LOCAL_PATH)/jni/3rdParty/TinyXml \
                    $(LOCAL_PATH)/jni/3rdParty/minizip \
                    $(LOCAL_PATH)/jni \
					                    					                    
LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb

LOCAL_STATIC_LIBRARIES := oculus jpeg

# Restore the local path since we overwrote it when we started
LOCAL_PATH := $(LOCAL_PATH_TEMP)
