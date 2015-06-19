// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LaunchPrivatePCH.h"
#include "ExceptionHandling.h"
#include "AndroidPlatformCrashContext.h"

#include "AndroidJNI.h"
#include "AndroidApplication.h"
#include <Android/asset_manager.h>
#include <Android/asset_manager_jni.h>

#define JNI_CURRENT_VERSION JNI_VERSION_1_6

JavaVM* GJavaVM;

// Pointer to target widget for virtual keyboard contents
static IVirtualKeyboardEntry *VirtualKeyboardWidget = NULL;

extern FString GFilePathBase;
extern FString GExternalFilePath;
extern FString GFontPathBase;
extern bool GOBBinAPK;

FOnActivityResult FJavaWrapper::OnActivityResultDelegate;

//////////////////////////////////////////////////////////////////////////

#if UE_BUILD_SHIPPING
// always clear any exceptions in SHipping
#define CHECK_JNI_RESULT(Id) if (Id == 0) { Env->ExceptionClear(); }
#else
#define CHECK_JNI_RESULT(Id) \
if (Id == 0) \
{ \
	if (bIsOptional) { Env->ExceptionClear(); } \
	else { Env->ExceptionDescribe(); checkf(Id != 0, TEXT("Failed to find " #Id)); } \
}
#endif

void FJavaWrapper::FindClassesAndMethods(JNIEnv* Env)
{
	bool bIsOptional = false;
	GameActivityClassID = FindClass(Env, "com/epicgames/ue4/GameActivity", bIsOptional);
	AndroidThunkJava_ShowConsoleWindow = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_ShowConsoleWindow", "(Ljava/lang/String;)V", bIsOptional);
	AndroidThunkJava_ShowVirtualKeyboardInput = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_ShowVirtualKeyboardInput", "(ILjava/lang/String;Ljava/lang/String;)V", bIsOptional);
	AndroidThunkJava_LaunchURL = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_LaunchURL", "(Ljava/lang/String;)V", bIsOptional);
	AndroidThunkJava_GetAssetManager = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_GetAssetManager", "()Landroid/content/res/AssetManager;", bIsOptional);
	AndroidThunkJava_Minimize = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_Minimize", "()V", bIsOptional);
	AndroidThunkJava_ForceQuit = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_ForceQuit", "()V", bIsOptional);
	AndroidThunkJava_GetFontDirectory = FindStaticMethod(Env, GameActivityClassID, "AndroidThunkJava_GetFontDirectory", "()Ljava/lang/String;", bIsOptional);
	AndroidThunkJava_Vibrate = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_Vibrate", "(I)V", bIsOptional);
	AndroidThunkJava_IsMusicActive = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_IsMusicActive", "()Z", bIsOptional);
	AndroidThunkJava_KeepScreenOn = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_KeepScreenOn", "(Z)V", bIsOptional);
	AndroidThunkJava_InitHMDs = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_InitHMDs", "()V", bIsOptional);
	AndroidThunkJava_IsGearVRApplication = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_IsGearVRApplication", "()Z", bIsOptional);


	// the rest are optional
	bIsOptional = true;
	// @todo split GooglePlay
	//	GoogleServicesClassID = FindClass(Env, "com/epicgames/ue4/GoogleServices", bIsOptional);
	GoogleServicesClassID = GameActivityClassID;
	AndroidThunkJava_ResetAchievements = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_ResetAchievements", "()V", bIsOptional);
	AndroidThunkJava_ShowAdBanner = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_ShowAdBanner", "(Ljava/lang/String;Z)V", bIsOptional);
	AndroidThunkJava_HideAdBanner = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_HideAdBanner", "()V", bIsOptional);
	AndroidThunkJava_CloseAdBanner = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_CloseAdBanner", "()V", bIsOptional);

	// In app purchase functionality
	AndroidThunkJava_IapSetupService = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapSetupService", "(Ljava/lang/String;)V", bIsOptional);
	AndroidThunkJava_IapQueryInAppPurchases = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapQueryInAppPurchases", "([Ljava/lang/String;[Z)Z", bIsOptional);
	AndroidThunkJava_IapBeginPurchase = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapBeginPurchase", "(Ljava/lang/String;Z)Z", bIsOptional);
	AndroidThunkJava_IapIsAllowedToMakePurchases = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapIsAllowedToMakePurchases", "()Z", bIsOptional);
}

jclass FJavaWrapper::FindClass(JNIEnv* Env, const ANSICHAR* ClassName, bool bIsOptional)
{
	jclass Class = Env->FindClass(ClassName);
	CHECK_JNI_RESULT(Class);
	return Class;
}

jmethodID FJavaWrapper::FindMethod(JNIEnv* Env, jclass Class, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, bool bIsOptional)
{
	jmethodID Method = Class == NULL ? NULL : Env->GetMethodID(Class, MethodName, MethodSignature);
	CHECK_JNI_RESULT(Method);
	return Method;
}

jmethodID FJavaWrapper::FindStaticMethod(JNIEnv* Env, jclass Class, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, bool bIsOptional)
{
	jmethodID Method = Class == NULL ? NULL : Env->GetStaticMethodID(Class, MethodName, MethodSignature);
	CHECK_JNI_RESULT(Method);
	return Method;
}

jfieldID FJavaWrapper::FindField(JNIEnv* Env, jclass Class, const ANSICHAR* FieldName, const ANSICHAR* FieldType, bool bIsOptional)
{
	jfieldID Field = Class == NULL ? NULL : Env->GetFieldID(Class, FieldName, FieldType);
	CHECK_JNI_RESULT(Field);
	return Field;
}

void FJavaWrapper::CallVoidMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	// make sure the function exists
	if (Method == NULL || Object == NULL)
	{
		return;
	}

	va_list Args;
	va_start(Args, Method);
	Env->CallVoidMethodV(Object, Method, Args);
	va_end(Args);
}

jobject FJavaWrapper::CallObjectMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	if (Method == NULL || Object == NULL)
	{
		return NULL;
	}

	va_list Args;
	va_start(Args, Method);
	jobject Return = Env->CallObjectMethodV(Object, Method, Args);
	va_end(Args);

	return Return;
}

bool FJavaWrapper::CallBooleanMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	if (Method == NULL || Object == NULL)
	{
		return false;
	}

	va_list Args;
	va_start(Args, Method);
	jboolean Return = Env->CallBooleanMethodV(Object, Method, Args);
	va_end(Args);

	return (bool)Return;
}

//Declare all the static members of the class defs 
jclass FJavaWrapper::GameActivityClassID;
jobject FJavaWrapper::GameActivityThis;
jmethodID FJavaWrapper::AndroidThunkJava_ShowConsoleWindow;
jmethodID FJavaWrapper::AndroidThunkJava_ShowVirtualKeyboardInput;
jmethodID FJavaWrapper::AndroidThunkJava_LaunchURL;
jmethodID FJavaWrapper::AndroidThunkJava_GetAssetManager;
jmethodID FJavaWrapper::AndroidThunkJava_Minimize;
jmethodID FJavaWrapper::AndroidThunkJava_ForceQuit;
jmethodID FJavaWrapper::AndroidThunkJava_GetFontDirectory;
jmethodID FJavaWrapper::AndroidThunkJava_Vibrate;
jmethodID FJavaWrapper::AndroidThunkJava_IsMusicActive;
jmethodID FJavaWrapper::AndroidThunkJava_KeepScreenOn;
jmethodID FJavaWrapper::AndroidThunkJava_InitHMDs;
jmethodID FJavaWrapper::AndroidThunkJava_IsGearVRApplication;

jclass FJavaWrapper::GoogleServicesClassID;
jobject FJavaWrapper::GoogleServicesThis;
jmethodID FJavaWrapper::AndroidThunkJava_ResetAchievements;
jmethodID FJavaWrapper::AndroidThunkJava_ShowAdBanner;
jmethodID FJavaWrapper::AndroidThunkJava_HideAdBanner;
jmethodID FJavaWrapper::AndroidThunkJava_CloseAdBanner;
jmethodID FJavaWrapper::AndroidThunkJava_IapSetupService;
jmethodID FJavaWrapper::AndroidThunkJava_IapQueryInAppPurchases;
jmethodID FJavaWrapper::AndroidThunkJava_IapBeginPurchase;
jmethodID FJavaWrapper::AndroidThunkJava_IapIsAllowedToMakePurchases;

//Game-specific crash reporter
void EngineCrashHandler(const FGenericCrashContext& GenericContext)
{
	const FAndroidCrashContext& Context = static_cast<const FAndroidCrashContext&>(GenericContext);

	static int32 bHasEntered = 0;
	if (FPlatformAtomics::InterlockedCompareExchange(&bHasEntered, 1, 0) == 0)
	{
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR* StackTrace = (ANSICHAR*)FMemory::Malloc(StackTraceSize);
		StackTrace[0] = 0;

		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, Context.Context);
		UE_LOG(LogEngine, Error, TEXT("%s"), ANSI_TO_TCHAR(StackTrace));
		FMemory::Free(StackTrace);

		GError->HandleError();
		FPlatformMisc::RequestExit(true);
	}
}

void AndroidThunkCpp_KeepScreenOn(bool Enable)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		// call the java side
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_KeepScreenOn, Enable);
	}
}

void AndroidThunkCpp_Vibrate(int32 Duration)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		// call the java side
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_Vibrate, Duration);
	}
}

// Call the Java side code for initializing VR HMD modules
void AndroidThunkCpp_InitHMDs()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_InitHMDs);
	}
}

// Check to see if this application is packaged for GearVR
bool AndroidThunkCpp_IsGearVRApplication()
{
	bool bIsGearVRApplication = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		bIsGearVRApplication = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_IsGearVRApplication);
	}

	return bIsGearVRApplication;
}

void AndroidThunkCpp_ShowConsoleWindow()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		// figure out all the possible texture formats that are allowed
		TArray<FString> PossibleTargetPlatforms;
		FPlatformMisc::GetValidTargetPlatforms(PossibleTargetPlatforms);

		// separate the format suffixes with commas
		FString ConsoleText;
		for (int32 FormatIndex = 0; FormatIndex < PossibleTargetPlatforms.Num(); FormatIndex++)
		{
			const FString& Format = PossibleTargetPlatforms[FormatIndex];
			int32 UnderscoreIndex;
			if (Format.FindLastChar('_', UnderscoreIndex))
			{
				if (ConsoleText != TEXT(""))
				{
					ConsoleText += ", ";
				}

				ConsoleText += Format.Mid(UnderscoreIndex + 1);
			}
		}

		// call the java side
		jstring ConsoleTextJava = Env->NewStringUTF(TCHAR_TO_UTF8(*ConsoleText));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_ShowConsoleWindow, ConsoleTextJava);
		Env->DeleteLocalRef(ConsoleTextJava);
	}
}

void AndroidThunkCpp_ShowVirtualKeyboardInput(TSharedPtr<IVirtualKeyboardEntry> TextWidget, int32 InputType, const FString& Label, const FString& Contents)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		// remember target widget for contents
		VirtualKeyboardWidget = &(*TextWidget);

		// call the java side
		jstring LabelJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Label));
		jstring ContentsJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Contents));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_ShowVirtualKeyboardInput, InputType, LabelJava, ContentsJava);
		Env->DeleteLocalRef(ContentsJava);
		Env->DeleteLocalRef(LabelJava);
	}
}

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeVirtualKeyboardResult(bool update, String contents);"
extern "C" void Java_com_epicgames_ue4_GameActivity_nativeVirtualKeyboardResult(JNIEnv* jenv, jobject thiz, jboolean update, jstring contents)
{
	// update text widget with new contents if OK pressed
	if (update == JNI_TRUE)
	{
		if (VirtualKeyboardWidget != NULL)
		{
			const char* javaChars = jenv->GetStringUTFChars(contents, 0);
			VirtualKeyboardWidget->SetTextFromVirtualKeyboard(FText::FromString(FString(UTF8_TO_TCHAR(javaChars))));

			//Release the string
			jenv->ReleaseStringUTFChars(contents, javaChars);
		}
	}

	// release reference
	VirtualKeyboardWidget = NULL;
}

void AndroidThunkCpp_LaunchURL(const FString& URL)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring Argument = Env->NewStringUTF(TCHAR_TO_UTF8(*URL));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_LaunchURL, Argument);
		Env->DeleteLocalRef(Argument);
	}
}

void AndroidThunkCpp_ResetAchievements()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_ResetAchievements);
	}
}

void AndroidThunkCpp_ShowAdBanner(const FString& AdUnitID, bool bShowOnBottomOfScreen)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		jstring AdUnitIDArg = Env->NewStringUTF(TCHAR_TO_UTF8(*AdUnitID));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_ShowAdBanner, AdUnitIDArg, bShowOnBottomOfScreen);
		Env->DeleteLocalRef(AdUnitIDArg);
	}
}

void AndroidThunkCpp_HideAdBanner()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_HideAdBanner);
 	}
}

void AndroidThunkCpp_CloseAdBanner()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_CloseAdBanner);
 	}
}

namespace
{
	jobject GJavaAssetManager = NULL;
	AAssetManager* GAssetManagerRef = NULL;
}

jobject AndroidJNI_GetJavaAssetManager()
{
	if (!GJavaAssetManager)
	{
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			GJavaAssetManager = FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_GetAssetManager);
			Env->NewGlobalRef(GJavaAssetManager);
		}
	}
	return GJavaAssetManager;
}

AAssetManager * AndroidThunkCpp_GetAssetManager()
{
	if (!GAssetManagerRef)
	{
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			jobject JavaAssetMgr = AndroidJNI_GetJavaAssetManager();
			GAssetManagerRef = AAssetManager_fromJava(Env, JavaAssetMgr);
		}
	}

	return GAssetManagerRef;
}

void AndroidThunkCpp_Minimize()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_Minimize);
	}
}

void AndroidThunkCpp_ForceQuit()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_ForceQuit);
	}
}

bool AndroidThunkCpp_IsMusicActive()
{
	bool bIsActive = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		bIsActive = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_IsMusicActive);
	}

	return bIsActive;
}

void AndroidThunkCpp_Iap_SetupIapService(const FString& InProductKey)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring ProductKey = Env->NewStringUTF(TCHAR_TO_UTF8(*InProductKey));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapSetupService, ProductKey);
		Env->DeleteLocalRef(ProductKey);
	}
}

bool AndroidThunkCpp_Iap_QueryInAppPurchases(const TArray<FString>& ProductIDs, const TArray<bool>& bConsumable)
{
	FPlatformMisc::LowLevelOutputDebugString(L"[JNI] - AndroidThunkCpp_Iap_QueryInAppPurchases");
	bool bResult = false;

	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jclass StringClass = Env->FindClass("java/lang/String");

		// Populate some java types with the provided product information
		jobjectArray ProductIDArray = (jobjectArray)Env->NewObjectArray(ProductIDs.Num(), StringClass, NULL);
		jbooleanArray ConsumeArray = (jbooleanArray)Env->NewBooleanArray(ProductIDs.Num());

		jboolean* ConsumeArrayValues = Env->GetBooleanArrayElements(ConsumeArray, 0);
		for (uint32 Param = 0; Param < ProductIDs.Num(); Param++)
		{
			jstring StringValue = Env->NewStringUTF(TCHAR_TO_UTF8(*ProductIDs[Param]));
			Env->SetObjectArrayElement(ProductIDArray, Param, StringValue);
			Env->DeleteLocalRef(StringValue);

			ConsumeArrayValues[Param] = bConsumable[Param];
		}
		Env->ReleaseBooleanArrayElements(ConsumeArray, ConsumeArrayValues, 0);

		// Execute the java code for this operation
		bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapQueryInAppPurchases, ProductIDArray, ConsumeArray);
		
		// clean up references
		Env->DeleteLocalRef(ProductIDArray);
		Env->DeleteLocalRef(ConsumeArray);
	}

	return bResult;
}

bool AndroidThunkCpp_Iap_BeginPurchase(const FString& ProductID, const bool bConsumable)
{
	FPlatformMisc::LowLevelOutputDebugString(L"[JNI] - AndroidThunkCpp_Iap_BeginPurchase");
	bool bResult = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring ProductIdJava = Env->NewStringUTF(TCHAR_TO_UTF8(*ProductID));
		bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapBeginPurchase, ProductIdJava, bConsumable);
		Env->DeleteLocalRef(ProductIdJava);
	}

	return bResult;
}

bool AndroidThunkCpp_Iap_IsAllowedToMakePurchases()
{
	FPlatformMisc::LowLevelOutputDebugString(L"[JNI] - AndroidThunkCpp_Iap_IsAllowedToMakePurchases");
	bool bResult = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapIsAllowedToMakePurchases);
	}
	return bResult;
}


//The JNI_OnLoad function is triggered by loading the game library from 
//the Java source file.
//	static
//	{
//		System.loadLibrary("MyGame");
//	}
//
// Use the JNI_OnLoad function to map all the class IDs and method IDs to their respective
// variables. That way, later when the Java functions need to be called, the IDs will be ready.
// It is much slower to keep looking up the class and method IDs.

JNIEXPORT jint JNI_OnLoad(JavaVM* InJavaVM, void* InReserved)
{
	FPlatformMisc::LowLevelOutputDebugString(L"In the JNI_OnLoad function");

	JNIEnv* Env = NULL;
	InJavaVM->GetEnv((void **)&Env, JNI_CURRENT_VERSION);

	// if you have problems with stuff being missing esspecially in distribution builds then it could be because proguard is stripping things from java
	// check proguard-project.txt and see if your stuff is included in the exceptions
	GJavaVM = InJavaVM;
	FAndroidApplication::InitializeJavaEnv(GJavaVM, JNI_CURRENT_VERSION, FJavaWrapper::GameActivityThis);

	FJavaWrapper::FindClassesAndMethods(Env);

	// hook signals
#if UE_BUILD_DEBUG
	if (GAlwaysReportCrash)
#else
	if (0)//!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash)
#endif
	{
		// disable crash handler.. getting better stack traces from system logcat for now
//		FPlatformMisc::SetCrashHandler(EngineCrashHandler);
	}

	// Cache path to external storage
	jclass EnvClass = Env->FindClass("android/os/Environment");
	jmethodID getExternalStorageDir = Env->GetStaticMethodID(EnvClass, "getExternalStorageDirectory", "()Ljava/io/File;");
	jobject externalStoragePath = Env->CallStaticObjectMethod(EnvClass, getExternalStorageDir, nullptr);
	jmethodID getFilePath = Env->GetMethodID(Env->FindClass("java/io/File"), "getPath", "()Ljava/lang/String;");
	jstring pathString = (jstring)Env->CallObjectMethod(externalStoragePath, getFilePath, nullptr);
	const char *nativePathString = Env->GetStringUTFChars(pathString, 0);
	// Copy that somewhere safe 
	GFilePathBase = FString(nativePathString);

	// then release...
	Env->ReleaseStringUTFChars(pathString, nativePathString);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Path found as '%s'\n"), *GFilePathBase);

	// Get the system font directory
	jstring fontPath = (jstring)Env->CallStaticObjectMethod(FJavaWrapper::GameActivityClassID, FJavaWrapper::AndroidThunkJava_GetFontDirectory);
	const char * nativeFontPathString = Env->GetStringUTFChars(fontPath, 0);
	GFontPathBase = FString(nativeFontPathString);
	Env->ReleaseStringUTFChars(fontPath, nativeFontPathString);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Font Path found as '%s'\n"), *GFontPathBase);

	// Wire up to core delegates, so core code can call out to Java
	DECLARE_DELEGATE_OneParam(FAndroidLaunchURLDelegate, const FString&);
	extern CORE_API FAndroidLaunchURLDelegate OnAndroidLaunchURL;
	OnAndroidLaunchURL = FAndroidLaunchURLDelegate::CreateStatic(&AndroidThunkCpp_LaunchURL);

	FPlatformMisc::LowLevelOutputDebugString(L"In the JNI_OnLoad function 5");


	return JNI_CURRENT_VERSION;
}

//Native-defined functions

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeSetGlobalActivity();"
extern "C" void Java_com_epicgames_ue4_GameActivity_nativeSetGlobalActivity(JNIEnv* jenv, jobject thiz /*, jobject googleServices*/)
{
	if (!FJavaWrapper::GameActivityThis)
	{
		FJavaWrapper::GameActivityThis = jenv->NewGlobalRef(thiz);
		FAndroidApplication::InitializeJavaEnv(GJavaVM, JNI_CURRENT_VERSION, FJavaWrapper::GameActivityThis);
		if (!FJavaWrapper::GameActivityThis)
		{
			FPlatformMisc::LowLevelOutputDebugString(L"Error setting the global GameActivity activity");
			check(false);
		}

		// @todo split GooglePlay, this needs to be passed in to this function
		FJavaWrapper::GoogleServicesThis = FJavaWrapper::GameActivityThis;
		// FJavaWrapper::GoogleServicesThis = jenv->NewGlobalRef(googleServices);

		bool bIsOptional = false;
		FJavaWrapper::GameActivityClassID = FJavaWrapper::FindClass(jenv, "com/epicgames/ue4/GameActivity", bIsOptional);

		// Next we check to see if the OBB file is in the APK
		jmethodID isOBBInAPKMethod = jenv->GetStaticMethodID(FJavaWrapper::GameActivityClassID, "isOBBInAPK", "()Z");
		GOBBinAPK = (bool)jenv->CallStaticBooleanMethod(FJavaWrapper::GameActivityClassID, isOBBInAPKMethod, nullptr);

		// Cache path to external files directory
		jclass ContextClass = jenv->FindClass("android/content/Context");
		jmethodID getExternalFilesDir = jenv->GetMethodID(ContextClass, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
		jobject externalFilesDirPath = jenv->CallObjectMethod(FJavaWrapper::GameActivityThis, getExternalFilesDir, nullptr);
		jmethodID getFilePath = jenv->GetMethodID(jenv->FindClass("java/io/File"), "getPath", "()Ljava/lang/String;");
		jstring externalFilesPathString = (jstring)jenv->CallObjectMethod(externalFilesDirPath, getFilePath, nullptr);
		const char *nativeExternalFilesPathString = jenv->GetStringUTFChars(externalFilesPathString, 0);
		// Copy that somewhere safe 
		GExternalFilePath = FString(nativeExternalFilesPathString);

		// then release...
		jenv->ReleaseStringUTFChars(externalFilesPathString, nativeExternalFilesPathString);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("ExternalFilePath found as '%s'\n"), *GExternalFilePath);
	}
}


extern "C" bool Java_com_epicgames_ue4_GameActivity_nativeIsShippingBuild(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
#if UE_BUILD_SHIPPING
	return JNI_TRUE;
#else
	return JNI_FALSE;
#endif
}

extern "C" void Java_com_epicgames_ue4_GameActivity_nativeOnActivityResult(JNIEnv* jenv, jobject thiz, jobject activity, jint requestCode, jint resultCode, jobject data)
{
	FJavaWrapper::OnActivityResultDelegate.Broadcast(jenv, thiz, activity, requestCode, resultCode, data);
}
