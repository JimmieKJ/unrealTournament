LOCAL_PATH := $(call my-dir)

# jni is always prepended to this, unfortunately
OCULUS := ../../VRLib/jni

include $(CLEAR_VARS)				# clean everything up to prepare for a module

APP_MODULE := oculus

LOCAL_MODULE    := oculus			# generate liboculus.so

LOCAL_ARM_MODE  := arm				# full speed arm instead of thumb
LOCAL_ARM_NEON  := true				# compile with neon support enabled

# Applications will need to link against these libraries, we
# can't link them into a static VrLib ourselves.
#
# LOCAL_LDLIBS	+= -lGLESv3			# OpenGL ES 3.0
# LOCAL_LDLIBS	+= -lEGL			# GL platform interface
# LOCAL_LDLIBS	+= -llog			# logging
# LOCAL_LDLIBS	+= -landroid		# native windows

include $(LOCAL_PATH)/../cflags.mk

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(OCULUS)/LibOVR/Include \
                    $(LOCAL_PATH)/$(OCULUS)/LibOVR/Src \
                    $(LOCAL_PATH)/$(OCULUS)/3rdParty/minizip \
                    $(LOCAL_PATH)/$(OCULUS)
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

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
                    LibOVR/Src/Kernel/OVR_ThreadCommandQueue.cpp \
                    LibOVR/Src/Kernel/OVR_ThreadsPthread.cpp \
                    LibOVR/Src/Kernel/OVR_Timer.cpp \
                    LibOVR/Src/Kernel/OVR_UTF8Util.cpp \
                    LibOVR/Src/Kernel/OVR_JSON.cpp \
                    LibOVR/Src/Kernel/OVR_BinaryFile.cpp \
                    LibOVR/Src/Kernel/OVR_MappedFile.cpp \
                    LibOVR/Src/Kernel/OVR_MemBuffer.cpp \
                    LibOVR/Src/Android/GlUtils.cpp \
                    LibOVR/Src/Android/JniUtils.cpp \
                    LibOVR/Src/Android/LogUtils.cpp \
                    LibOVR/Src/Android/NativeBuildStrings.cpp \
                    LibOVR/Src/Android/OVRVersion.cpp \
                    VrApi/VrApi.cpp \
                    VrApi/Vsync.cpp \
                    VrApi/DirectRender.cpp \
                    VrApi/HmdInfo.cpp \
                    VrApi/HmdSensors.cpp \
                    VrApi/Distortion.cpp \
                    VrApi/SystemActivities.cpp \
                    VrApi/TimeWarp.cpp \
                    VrApi/TimeWarpProgs.cpp \
                    VrApi/ImageServer.cpp \
                    VrApi/LocalPreferences.cpp \
                    VrApi/WarpGeometry.cpp \
                    VrApi/WarpProgram.cpp \
                    VrApi/Sensors/OVR_DeviceHandle.cpp \
                    VrApi/Sensors/OVR_DeviceImpl.cpp \
                    VrApi/Sensors/OVR_LatencyTest.cpp \
                    VrApi/Sensors/OVR_LatencyTestDeviceImpl.cpp \
                    VrApi/Sensors/OVR_Profile.cpp \
                    VrApi/Sensors/OVR_SensorFilter.cpp \
                    VrApi/Sensors/OVR_SensorCalibration.cpp \
                    VrApi/Sensors/OVR_GyroTempCalibration.cpp \
                    VrApi/Sensors/OVR_SensorFusion.cpp \
                    VrApi/Sensors/OVR_SensorTimeFilter.cpp \
                    VrApi/Sensors/OVR_SensorDeviceImpl.cpp \
                    VrApi/Sensors/OVR_Android_DeviceManager.cpp \
                    VrApi/Sensors/OVR_Android_HIDDevice.cpp \
                    VrApi/Sensors/OVR_Android_HMDDevice.cpp \
                    VrApi/Sensors/OVR_Android_SensorDevice.cpp \
                    VrApi/Sensors/OVR_Android_PhoneSensors.cpp \
                    VrApi/Sensors/OVR_Stereo.cpp \
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
                    VRMenu/TextFade_Component.cpp \
                    VRMenu/CollisionPrimitive.cpp \
                    VRMenu/ActionComponents.cpp \
                    VRMenu/AnimComponents.cpp \
                    VRMenu/VolumePopup.cpp \
                    VRMenu/ScrollManager.cpp \
                    VRMenu/ScrollBarComponent.cpp \
                    VRMenu/SwipeHintComponent.cpp \
                    VRMenu/MetaDataManager.cpp \
					VRMenu/OutOfSpaceMenu.cpp \
                    BitmapFont.cpp \
                    ImageData.cpp \
                    GlSetup.cpp \
                    GlTexture.cpp \
                    GlProgram.cpp \
                    GlGeometry.cpp \
                    PackageFiles.cpp \
                    SurfaceTexture.cpp \
                    VrCommon.cpp \
                    EyeBuffers.cpp \
                    MessageQueue.cpp \
                    TalkToJava.cpp \
                    KeyState.cpp \
                    App.cpp \
                    AppRender.cpp \
                    PathUtils.cpp \
                    EyePostRender.cpp \
                    ModelRender.cpp \
                    ModelFile.cpp \
                    ModelCollision.cpp \
                    ModelTrace.cpp \
                    ModelView.cpp \
                    DebugLines.cpp \
                    GazeCursor.cpp \
                    SwipeView.cpp \
                    SoundManager.cpp \
                    UserProfile.cpp \
                    VrLocale.cpp \
                    Console.cpp


LOCAL_SRC_FILES +=	3rdParty/stb/stb_image.c \
					3rdParty/stb/stb_image_write.c

# minizip for loading ovrscene files
LOCAL_SRC_FILES +=	3rdParty/minizip/ioapi.c \
					3rdParty/minizip/miniunz.c \
					3rdParty/minizip/mztools.c \
					3rdParty/minizip/unzip.c \
					3rdParty/minizip/zip.c

# OVR::Capture support...
LOCAL_C_INCLUDES  += $(LOCAL_PATH)/LibOVR/Src/Capture/include
LOCAL_SRC_FILES   += $(wildcard $(realpath $(LOCAL_PATH))/LibOVR/Src/Capture/src/*.cpp)
LOCAL_CFLAGS      += -DOVR_ENABLE_CAPTURE=1


# OpenGL ES 3.0
LOCAL_EXPORT_LDLIBS := -lGLESv3
# GL platform interface
LOCAL_EXPORT_LDLIBS += -lEGL
# native multimedia
LOCAL_EXPORT_LDLIBS += -lOpenMAXAL
# logging
LOCAL_EXPORT_LDLIBS += -llog
# native windows
LOCAL_EXPORT_LDLIBS += -landroid
# For minizip
LOCAL_EXPORT_LDLIBS += -lz
# audio
LOCAL_EXPORT_LDLIBS += -lOpenSLES

include $(BUILD_STATIC_LIBRARY)		# start building based on everything since CLEAR_VARS

#$(call import-module,android-ndk-profiler)
