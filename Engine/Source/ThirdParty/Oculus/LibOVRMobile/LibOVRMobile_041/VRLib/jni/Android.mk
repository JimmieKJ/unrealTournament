LOCAL_PATH := $(call my-dir)

# jni is always prepended to this, unfortunately
OCULUS := ../../VRLib/jni

# jpeg
include $(CLEAR_VARS)

LOCAL_MODULE          := jpeg
LOCAL_SRC_FILES := $(OCULUS)/3rdParty/libjpeg.a

include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)				# clean everything up to prepare for a module

APP_MODULE := oculus



LOCAL_MODULE    := oculus			# generate liboculus.so

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb

# Applications will need to link against these libraries, we
# can't link them into a static VrLib ourselves.
#
# LOCAL_LDLIBS	+= -lGLESv3			# OpenGL ES 3.0
# LOCAL_LDLIBS	+= -lEGL			# GL platform interface
# LOCAL_LDLIBS	+= -llog			# logging
# LOCAL_LDLIBS	+= -landroid		# native windows

include cflags.mk

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(OCULUS)/LibOVR/Include \
                    $(LOCAL_PATH)/$(OCULUS)/LibOVR/Src \
                    $(LOCAL_PATH)/$(OCULUS)/3rdParty/minizip \
                    $(LOCAL_PATH)/$(OCULUS)
					                    					                    
LOCAL_SRC_FILES  := LibOVR/Src/Kernel/OVR_Alg.cpp \
                    LibOVR/Src/Kernel/OVR_Allocator.cpp \
                    LibOVR/Src/Kernel/OVR_Atomic.cpp \
                    LibOVR/Src/Kernel/OVR_File.cpp \
                    LibOVR/Src/Kernel/OVR_FileFILE.cpp \
                    LibOVR/Src/Kernel/OVR_Log.cpp \
                    LibOVR/Src/Kernel/OVR_Lockless.cpp \
                    LibOVR/Src/Kernel/OVR_Math.cpp \
                    LibOVR/Src/Kernel/OVR_RefCount.cpp \
                    LibOVR/Src/Kernel/OVR_Std.cpp \
                    LibOVR/Src/Kernel/OVR_String.cpp \
                    LibOVR/Src/Kernel/OVR_String_FormatUtil.cpp \
                    LibOVR/Src/Kernel/OVR_String_PathUtil.cpp \
                    LibOVR/Src/Kernel/OVR_SysFile.cpp \
                    LibOVR/Src/Kernel/OVR_System.cpp \
                    LibOVR/Src/Kernel/OVR_ThreadsPthread.cpp \
                    LibOVR/Src/Kernel/OVR_Timer.cpp \
                    LibOVR/Src/Kernel/OVR_UTF8Util.cpp \
                    LibOVR/Src/Util/Util_LatencyTest.cpp \
                    LibOVR/Src/CAPI/CAPI_GlobalState.cpp \
                    LibOVR/Src/CAPI/CAPI_HMDState.cpp \
                    LibOVR/Src/OVR_CAPI.cpp \
                    LibOVR/Src/OVR_DeviceHandle.cpp \
                    LibOVR/Src/OVR_DeviceImpl.cpp \
                    LibOVR/Src/OVR_JSON.cpp \
                    LibOVR/Src/OVR_BinaryFile.cpp \
                    LibOVR/Src/OVR_MappedFile.cpp \
                    LibOVR/Src/OVR_LatencyTestImpl.cpp \
                    LibOVR/Src/OVR_Profile.cpp \
                    LibOVR/Src/OVR_SensorFilter.cpp \
                    LibOVR/Src/OVR_SensorCalibration.cpp \
                    LibOVR/Src/OVR_GyroTempCalibration.cpp \
                    LibOVR/Src/OVR_SensorFusion.cpp \
                    LibOVR/Src/OVR_SensorTimeFilter.cpp \
                    LibOVR/Src/OVR_SensorImpl.cpp \
                    LibOVR/Src/OVR_ThreadCommandQueue.cpp \
                    LibOVR/Src/OVR_Android_DeviceManager.cpp \
                    LibOVR/Src/OVR_Android_HIDDevice.cpp \
                    LibOVR/Src/OVR_Android_HMDDevice.cpp \
                    LibOVR/Src/OVR_Android_SensorDevice.cpp \
                    LibOVR/Src/OVR_Android_PhoneSensors.cpp \
                    LibOVR/Src/OVR_Stereo.cpp \
					RayTracer/RtIntersect.cpp \
					RayTracer/RtTrace.cpp \
					VRMenu/VRMenuComponent.cpp \
					VRMenu/VRMenuMgr.cpp \
					VRMenu/VRMenuObjectLocal.cpp \
					VRMenu/VRMenuEvent.cpp \
					VRMenu/VRMenuEventHandler.cpp \
					VRMenu/SoundLimiter.cpp \
					VRMenu/VRMenu.cpp \
					VRMenu/GuiSys.cpp \
					VRMenu/FolderBrowser.cpp \
					VRMenu/Fader.cpp \
					VRMenu/DefaultComponent.cpp \
					VRMenu/GlobalMenu.cpp \
					VRMenu/TextFade_Component.cpp \
					VRMenu/CollisionPrimitive.cpp \
					VRMenu/ActionComponents.cpp \
					VRMenu/AnimComponents.cpp \
					VRMenu/VolumePopup.cpp \
					VRMenu/SliderComponent.cpp \
					VrApi/VrApi.cpp \
                    VrApi/Vsync.cpp \
                    VrApi/DirectRender.cpp \
                    VrApi/HmdInfo.cpp \
                    VrApi/Distortion.cpp \
                    VrApi/TimeWarp.cpp \
                    VrApi/ImageServer.cpp \
                    VrApi/LocalPreferences.cpp \
                    VrApi/NativeBuildStrings.cpp \
					VrApi/JniUtils.cpp \
					BitmapFont.cpp \
					ImageData.cpp \
                    GlUtils.cpp \
                    GlTexture.cpp \
                    GlProgram.cpp \
                    GlGeometry.cpp \
                    Log.cpp \
                    PackageFiles.cpp \
                    SurfaceTexture.cpp \
                    VrCommon.cpp \
                    EyeBuffers.cpp \
                    MessageQueue.cpp \
                    TalkToJava.cpp \
					KeyState.cpp \
                    App.cpp \
                    AppRender.cpp \
                    PlatformActivity.cpp \
                    EyePostRender.cpp \
                    MemBuffer.cpp \
                    ModelRender.cpp \
                    ModelFile.cpp \
					ModelCollision.cpp \
                    ModelView.cpp \
                    DebugLines.cpp \
					GazeCursor.cpp \
					SwipeView.cpp \
					SwipeDir.cpp \
					SearchPaths.cpp \
					SoundManager.cpp
					
LOCAL_SRC_FILES +=	3rdParty/stb/stb_image.c \
					3rdParty/stb/stb_image_write.c

# minizip for loading ovrscene files
LOCAL_SRC_FILES +=	3rdParty/minizip/ioapi.c \
					3rdParty/minizip/miniunz.c \
					3rdParty/minizip/mztools.c \
					3rdParty/minizip/unzip.c \
					3rdParty/minizip/zip.c
					
include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

#--------------------------------------------------------
# Unity plugin
#--------------------------------------------------------
#include $(CLEAR_VARS)
#
#LOCAL_MODULE := OculusPlugin 
#
#LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(OCULUS)/LibOVR/Include \
#                    $(LOCAL_PATH)/$(OCULUS)/LibOVR/Src \
#                    $(LOCAL_PATH)/$(OCULUS)/3rdParty/minizip \
#                    $(LOCAL_PATH)/$(OCULUS)
#
#LOCAL_STATIC_LIBRARIES := oculus jpeg
##LOCAL_STATIC_LIBRARIES += android-ndk-profiler
#
#LOCAL_LDLIBS	+= -lGLESv3			# OpenGL ES 3.0
#LOCAL_LDLIBS	+= -lEGL			# GL platform interface
#LOCAL_LDLIBS  	+= -llog			# logging
#LOCAL_LDLIBS  	+= -landroid		# native windows
#LOCAL_LDLIBS	+= -lz				# For minizip
#LOCAL_LDLIBS	+= -lOpenSLES		# audio
#
#LOCAL_SRC_FILES  := $(OCULUS)/Integrations/Unity/UnityPlugin.cpp \
#                    $(OCULUS)/Integrations/Unity/MediaSurface.cpp \
#                    $(OCULUS)/Integrations/Unity/SensorPlugin.cpp \
#                    $(OCULUS)/Integrations/Unity/RenderingPlugin.cpp
#
#include $(BUILD_SHARED_LIBRARY)

#--------------------------------------------------------
# JavaVr.so
#
# This .so can be loaded by a java project that wants to
# do frame and eye rendering completely in java without
# needing the NDK.
#--------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := JavaVr

LOCAL_C_INCLUDES += $(LOCAL_PATH)/$(OCULUS)/LibOVR/Include \
                    $(LOCAL_PATH)/$(OCULUS)/LibOVR/Src \
                    $(LOCAL_PATH)/$(OCULUS)/3rdParty/minizip \
                    $(LOCAL_PATH)/$(OCULUS)

LOCAL_STATIC_LIBRARIES := oculus jpeg
#LOCAL_STATIC_LIBRARIES += android-ndk-profiler

LOCAL_LDLIBS	+= -lGLESv3			# OpenGL ES 3.0
LOCAL_LDLIBS	+= -lEGL			# GL platform interface
LOCAL_LDLIBS  	+= -llog			# logging
LOCAL_LDLIBS  	+= -landroid		# native windows
LOCAL_LDLIBS	+= -lz				# For minizip
LOCAL_LDLIBS	+= -lOpenSLES		# audio

LOCAL_SRC_FILES  := $(OCULUS)/Integrations/PureJava/PureJava.cpp

include $(BUILD_SHARED_LIBRARY)


#$(call import-module,android-ndk-profiler)
