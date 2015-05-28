// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	static jmethodID AndroidThunkJava_IsGearVRApplication;

	// IDs related to google play services
	static jclass GoogleServicesClassID;
	static jobject GoogleServicesThis;
	static jmethodID AndroidThunkJava_ResetAchievements;
	static jmethodID AndroidThunkJava_ShowAdBanner;
	static jmethodID AndroidThunkJava_HideAdBanner;
	static jmethodID AndroidThunkJava_CloseAdBanner;

	// In app purchase functionality
	static jmethodID AndroidThunkJava_IapSetupService;
	static jmethodID AndroidThunkJava_IapQueryInAppPurchases;
	static jmethodID AndroidThunkJava_IapBeginPurchase;
	static jmethodID AndroidThunkJava_IapIsAllowedToMakePurchases;

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
	static bool CallBooleanMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);

	// Delegate that can be registered to that is called when an activity is finished
	static FOnActivityResult OnActivityResultDelegate;
};
