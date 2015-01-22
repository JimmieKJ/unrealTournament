/************************************************************************************

Filename    :   JavaVM.h
Content     :   
Created     :   July 14, 2014
Authors     :   John Carmack

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/

#include <jni.h>

// This is set by JNI_OnLoad() when the .so is initially loaded.
// Must use to attach each thread that will use JNI:
// VrLibJavaVM->AttachCurrentThread(&JNIEnv, 0);
namespace OVR
{
extern JavaVM	* VrLibJavaVM;
}
