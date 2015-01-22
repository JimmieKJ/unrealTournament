/************************************************************************************

Filename    :   JniUtils.h
Content     :   JNI Utility functions
Created     :   October 21, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright 2014 Oculus VR, LLC. All Rights reserved.


*************************************************************************************/
#ifndef OVR_JniUtils_h
#define OVR_JniUtils_h

#include <jni.h>

jclass		ovr_GetGlobalClassReference( JNIEnv * jni, const char * className );
jmethodID	ovr_GetStaticMethodID( JNIEnv * jni, jclass jniclass, const char * name, const char * signature );

void		ovr_LoadDevConfig( bool const forceReload );

// Tries to read the Home package name from the dev config. Defaults to "com.oculus.home".
const char * ovr_GetHomePackageName( char * packageName, int const maxLen );

// Get the current package name, for instance "com.oculus.home".
const char * ovr_GetCurrentPackageName( JNIEnv * jni, jobject activityObject, char * packageName, int const maxLen );

// Get the current activity class name, for instance "com.oculusvr.vrlib.PlatformActivity".
const char * ovr_GetCurrentActivityName( JNIEnv * jni, jobject activityObject, char * className, int const maxLen );

// For instance packageName = "com.oculus.home".
bool ovr_IsCurrentPackage( JNIEnv * jni, jobject activityObject, const char * packageName );

// For instance className = "com.oculusvr.vrlib.PlatformActivity".
bool ovr_IsCurrentActivity( JNIEnv * jni, jobject activityObject, const char * className );

// Returns true if this is the Home package.
bool ovr_IsOculusHomePackage( JNIEnv * jni, jobject activityObject );

#endif	// OVR_JniUtils_h
