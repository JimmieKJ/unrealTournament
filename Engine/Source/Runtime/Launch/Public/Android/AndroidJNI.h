// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <jni.h>
#include <android/log.h>

extern JavaVM* GJavaVM;

DECLARE_MULTICAST_DELEGATE_SixParams(FOnActivityResult, JNIEnv *, jobject, jobject, jint, jint, jobject);

// Define all the Java classes/methods that the game will need to access to
class FJavaWrapper
{
public:

	// Nonstatic methods
	static jclass GameActivityClassID;
	static jobject GameActivityThis;
	static jmethodID AndroidThunkJava_ShowConsoleWindow;
	static jmethodID AndroidThunkJava_ShowVirtualKeyboardInput;
	static jmethodID AndroidThunkJava_LaunchURL;
	static jmethodID AndroidThunkJava_GetAssetManager;
	static jmethodID AndroidThunkJava_Minimize;
	static jmethodID AndroidThunkJava_ForceQuit;
	static jmethodID AndroidThunkJava_GetFontDirectory;
	static jmethodID AndroidThunkJava_Vibrate;
	static jmethodID AndroidThunkJava_IsMusicActive;
	static jmethodID AndroidThunkJava_KeepScreenOn;
	static jmethodID AndroidThunkJava_InitHMDs;
	static jmethodID AndroidThunkJava_DismissSplashScreen;
	static jmethodID AndroidThunkJava_GetInputDeviceInfo;
	static jmethodID AndroidThunkJava_HasMetaDataKey;
	static jmethodID AndroidThunkJava_GetMetaDataBoolean;
	static jmethodID AndroidThunkJava_GetMetaDataInt;
	static jmethodID AndroidThunkJava_GetMetaDataString;
	static jmethodID AndroidThunkJava_IsGearVRApplication;
	static jmethodID AndroidThunkJava_ShowHiddenAlertDialog;

	// InputDeviceInfo member field ids
	static jclass InputDeviceInfoClass;
	static jfieldID InputDeviceInfo_VendorId;
	static jfieldID InputDeviceInfo_ProductId;
	static jfieldID InputDeviceInfo_ControllerId;
	static jfieldID InputDeviceInfo_Name;
	static jfieldID InputDeviceInfo_Descriptor;

	// IDs related to google play services
	static jclass GoogleServicesClassID;
	static jobject GoogleServicesThis;
	static jmethodID AndroidThunkJava_ResetAchievements;
	static jmethodID AndroidThunkJava_ShowAdBanner;
	static jmethodID AndroidThunkJava_HideAdBanner;
	static jmethodID AndroidThunkJava_CloseAdBanner;
	static jmethodID AndroidThunkJava_GoogleClientConnect;
	static jmethodID AndroidThunkJava_GoogleClientDisconnect;

	// In app purchase functionality
	static jclass JavaStringClass;
	static jmethodID AndroidThunkJava_IapSetupService;
	static jmethodID AndroidThunkJava_IapQueryInAppPurchases;
	static jmethodID AndroidThunkJava_IapBeginPurchase;
	static jmethodID AndroidThunkJava_IapIsAllowedToMakePurchases;
	static jmethodID AndroidThunkJava_IapRestorePurchases;

	// SurfaceView functionality for view scaling on some devices
	static jmethodID AndroidThunkJava_UseSurfaceViewWorkaround;
	static jmethodID AndroidThunkJava_SetDesiredViewSize;

	/**
	 * Find all known classes and methods
	 */
	static void FindClassesAndMethods(JNIEnv* Env);

	/**
	 * Helper wrapper functions around the JNIEnv versions with NULL/error handling
	 */
	static jclass FindClass(JNIEnv* Env, const ANSICHAR* ClassName, bool bIsOptional);
	static jmethodID FindMethod(JNIEnv* Env, jclass Class, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, bool bIsOptional);
	static jmethodID FindStaticMethod(JNIEnv* Env, jclass Class, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, bool bIsOptional);
	static jfieldID FindField(JNIEnv* Env, jclass Class, const ANSICHAR* FieldName, const ANSICHAR* FieldType, bool bIsOptional);

	static void CallVoidMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);
	static jobject CallObjectMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);
	static int32 CallIntMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);
	static bool CallBooleanMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);

	// Delegate that can be registered to that is called when an activity is finished
	static FOnActivityResult OnActivityResultDelegate;
};
