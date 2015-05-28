// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//Google Play Services

#include "OnlineSubsystemGooglePlayPrivatePCH.h"
#include "AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OnlineAsyncTaskManagerGooglePlay.h"
#include "OnlineAsyncTaskGooglePlayLogin.h"

#include <android_native_app_glue.h>

#include "gpg/android_initialization.h"
#include "gpg/android_platform_configuration.h"
#include "gpg/builder.h"
#include "gpg/debug.h"
#include "gpg/android_support.h"

using namespace gpg;

FOnlineSubsystemGooglePlay::FOnlineSubsystemGooglePlay()
	: IdentityInterface(nullptr)
	, LeaderboardsInterface(nullptr)
	, AchievementsInterface(nullptr)
	, StoreInterface(nullptr)
	, CurrentLoginTask(nullptr)
{

}

IOnlineIdentityPtr FOnlineSubsystemGooglePlay::GetIdentityInterface() const
{
	return IdentityInterface;
}

IOnlineStorePtr FOnlineSubsystemGooglePlay::GetStoreInterface() const
{
	return StoreInterface;
}

IOnlineSessionPtr FOnlineSubsystemGooglePlay::GetSessionInterface() const
{
	return nullptr;
}

IOnlineFriendsPtr FOnlineSubsystemGooglePlay::GetFriendsInterface() const
{
	return nullptr;
}

IOnlinePartyPtr FOnlineSubsystemGooglePlay::GetPartyInterface() const
{
	return nullptr;
}

IOnlineGroupsPtr FOnlineSubsystemGooglePlay::GetGroupsInterface() const
{
	return nullptr;
}

IOnlineSharedCloudPtr FOnlineSubsystemGooglePlay::GetSharedCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemGooglePlay::GetUserCloudInterface() const
{
	return nullptr;
}

IOnlineUserCloudPtr FOnlineSubsystemGooglePlay::GetUserCloudInterface(const FString& Key) const
{
	return nullptr;
}

IOnlineLeaderboardsPtr FOnlineSubsystemGooglePlay::GetLeaderboardsInterface() const
{
	return LeaderboardsInterface;
}

IOnlineVoicePtr FOnlineSubsystemGooglePlay::GetVoiceInterface() const
{
	return nullptr;
}

IOnlineExternalUIPtr FOnlineSubsystemGooglePlay::GetExternalUIInterface() const
{
	return ExternalUIInterface;
}

IOnlineTimePtr FOnlineSubsystemGooglePlay::GetTimeInterface() const
{
	return nullptr;
}

IOnlineTitleFilePtr FOnlineSubsystemGooglePlay::GetTitleFileInterface() const
{
	return nullptr;
}

IOnlineEntitlementsPtr FOnlineSubsystemGooglePlay::GetEntitlementsInterface() const
{
	return nullptr;
}

IOnlineAchievementsPtr FOnlineSubsystemGooglePlay::GetAchievementsInterface() const
{
	return AchievementsInterface;
}

static bool WaitForLostFocus = false;
static bool WaitingForLogin = false;

bool FOnlineSubsystemGooglePlay::Init() 
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FOnlineSubsystemAndroid::Init"));
	
	OnlineAsyncTaskThreadRunnable.Reset(new FOnlineAsyncTaskManagerGooglePlay);
	OnlineAsyncTaskThread.Reset(FRunnableThread::Create(OnlineAsyncTaskThreadRunnable.Get(), TEXT("OnlineAsyncTaskThread")));

	IdentityInterface = MakeShareable(new FOnlineIdentityGooglePlay(this));
	LeaderboardsInterface = MakeShareable(new FOnlineLeaderboardsGooglePlay(this));
	AchievementsInterface = MakeShareable(new FOnlineAchievementsGooglePlay(this));
	ExternalUIInterface = MakeShareable(new FOnlineExternalUIGooglePlay(this));

	if (IsInAppPurchasingEnabled())
	{
		StoreInterface = MakeShareable(new FOnlineStoreGooglePlay(this));
	}
	
	extern struct android_app* GNativeAndroidApp;
	check(GNativeAndroidApp != nullptr);
	AndroidInitialization::android_main(GNativeAndroidApp);

	extern jobject GJavaGlobalNativeActivity;
	AndroidPlatformConfiguration PlatformConfiguration;
	PlatformConfiguration.SetActivity(GNativeAndroidApp->activity->clazz);

	// Queue up a task for the login so that other tasks execute after it.
	check(CurrentLoginTask == nullptr);
	CurrentLoginTask = new FOnlineAsyncTaskGooglePlayLogin(this, 0);
	QueueAsyncTask(CurrentLoginTask);

	WaitingForLogin = true;
	WaitForLostFocus = false;

	OnActivityResultDelegateHandle = FJavaWrapper::OnActivityResultDelegate.AddRaw(this, &FOnlineSubsystemGooglePlay::OnActivityResult);

	// Create() returns a std::unqiue_ptr, but we convert it to a TUniquePtr.
	GameServicesPtr.Reset( GameServices::Builder()
		.SetDefaultOnLog(LogLevel::VERBOSE)
		.SetOnAuthActionStarted([](AuthOperation op) {
			UE_LOG(LogOnline, Log, TEXT("GPG sign in started"));
		})
		.SetOnAuthActionFinished([this](AuthOperation Op, AuthStatus Status) {
			OnAuthActionFinished(Op, Status);

			if (Op == AuthOperation::SIGN_IN)
			{
				if (Status != AuthStatus::VALID)
				{
					WaitForLostFocus = true;
				}
				WaitingForLogin = false;
			}
		})
		.Create(PlatformConfiguration).release() );

	// Wait for GooglePlay to complete login task
	while (WaitingForLogin)
	{
		FPlatformProcess::Sleep(0.01f);
	}
	if (WaitForLostFocus)
	{
		extern bool WaitForAndroidLoseFocusEvent(double TimeoutSeconds);
		if (!WaitForAndroidLoseFocusEvent(3.0))
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("FOnlineSubsystemAndroid::Init - timed out"));
			OnAuthActionFinished(AuthOperation::SIGN_IN, AuthStatus::ERROR_NOT_AUTHORIZED);
		}
	}

	return true;
}

bool FOnlineSubsystemGooglePlay::Tick(float DeltaTime)
{
	if (!FOnlineSubsystemImpl::Tick(DeltaTime))
	{
		return false;
	}

	if (OnlineAsyncTaskThreadRunnable)
	{
		OnlineAsyncTaskThreadRunnable->GameTick();
	}

	return true;
}


bool FOnlineSubsystemGooglePlay::Shutdown() 
{
	UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemAndroid::Shutdown()"));

	FJavaWrapper::OnActivityResultDelegate.Remove(OnActivityResultDelegateHandle);

#define DESTRUCT_INTERFACE(Interface) \
	if (Interface.IsValid()) \
	{ \
	ensure(Interface.IsUnique()); \
	Interface = NULL; \
	}

	// Destruct the interfaces
	DESTRUCT_INTERFACE(StoreInterface);
	DESTRUCT_INTERFACE(ExternalUIInterface);
	DESTRUCT_INTERFACE(AchievementsInterface);
	DESTRUCT_INTERFACE(LeaderboardsInterface);
	DESTRUCT_INTERFACE(IdentityInterface);
#undef DESTRUCT_INTERFACE

	OnlineAsyncTaskThread.Reset();
	OnlineAsyncTaskThreadRunnable.Reset();

	return true;
}


FString FOnlineSubsystemGooglePlay::GetAppId() const 
{
	//get app id from settings. 
	return TEXT( "AndroidAppIDPlaceHolder" );
}


bool FOnlineSubsystemGooglePlay::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) 
{
	return false;
}

bool FOnlineSubsystemGooglePlay::IsEnabled(void)
{
	bool bEnabled = true;
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bEnableGooglePlaySupport"), bEnabled, GEngineIni);
	return bEnabled;
}

bool FOnlineSubsystemGooglePlay::IsInAppPurchasingEnabled()
{
	bool bEnabledIAP = false;
	GConfig->GetBool(TEXT("OnlineSubsystemGooglePlay.Store"), TEXT("bSupportsInAppPurchasing"), bEnabledIAP, GEngineIni);
	return bEnabledIAP;
}

void FOnlineSubsystemGooglePlay::QueueAsyncTask(FOnlineAsyncTask* AsyncTask)
{
	check(OnlineAsyncTaskThreadRunnable);
	OnlineAsyncTaskThreadRunnable->AddToInQueue(AsyncTask);
}

void FOnlineSubsystemGooglePlay::OnAuthActionFinished(AuthOperation Op, AuthStatus Status)
{
	FString OpString(DebugString(Op).c_str());
	FString StatusString(DebugString(Status).c_str());
	UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemGooglePlay::OnAuthActionFinished, Op: %s, Status: %s"), *OpString, *StatusString);

	if (Op == AuthOperation::SIGN_IN)
	{
		if (CurrentLoginTask == nullptr)
		{
			UE_LOG(LogOnline, Warning, TEXT("FOnlineSubsystemGooglePlay::OnAuthActionFinished: sign-in operation with a null CurrentLoginTask"));
			return;
		}

		CurrentLoginTask->OnAuthActionFinished(Op, Status);

		if (CurrentLoginTask->bIsComplete)
		{
			// Async task manager owns the task now and is responsible for cleaning it up.
			CurrentLoginTask = nullptr;
		}
	}
}

std::string FOnlineSubsystemGooglePlay::ConvertFStringToStdString(const FString& InString)
{
	int32 SrcLen  = InString.Len() + 1;
	int32 DestLen = FPlatformString::ConvertedLength<ANSICHAR>(*InString, SrcLen);
	TArray<ANSICHAR> Converted;
	Converted.AddUninitialized(DestLen);
	
	FPlatformString::Convert(Converted.GetData(), DestLen, *InString, SrcLen);

	return std::string(Converted.GetData());
}

void FOnlineSubsystemGooglePlay::OnActivityResult(JNIEnv *env, jobject thiz, jobject activity, jint requestCode, jint resultCode, jobject data)
{
	// Pass the result on to google play - otherwise, some callbacks for the turn based system do not get called.
	AndroidSupport::OnActivityResult(env, activity, requestCode, resultCode, data);
}
