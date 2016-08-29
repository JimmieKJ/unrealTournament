// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//Google Play Services

#include "OnlineSubsystemGooglePlayPrivatePCH.h"
#include "AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "OnlineAsyncTaskManagerGooglePlay.h"
#include "OnlineAsyncTaskGooglePlayLogin.h"
#include "OnlineAsyncTaskGooglePlayLogout.h"
#include "OnlineAsyncTaskGooglePlayShowLoginUI.h"

#include <android_native_app_glue.h>

#include "gpg/android_initialization.h"
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
	, CurrentShowLoginUITask(nullptr)
	, CurrentLogoutTask(nullptr)
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
	OnlineAsyncTaskThread.Reset(FRunnableThread::Create(OnlineAsyncTaskThreadRunnable.Get(), *FString::Printf(TEXT("OnlineAsyncTaskThread %s"), *InstanceName.ToString())));

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
	PlatformConfiguration.SetActivity(GNativeAndroidApp->activity->clazz);

	OnActivityResultDelegateHandle = FJavaWrapper::OnActivityResultDelegate.AddRaw(this, &FOnlineSubsystemGooglePlay::OnActivityResult);

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

	FOnlineSubsystemImpl::Shutdown();

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
	if (FOnlineSubsystemImpl::Exec(InWorld, Cmd, Ar))
	{
		return true;
	}
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

void FOnlineSubsystemGooglePlay::StartShowLoginUITask(int PlayerId, const FOnLoginUIClosedDelegate& Delegate)
{
	if (AreAnyAsyncLoginTasksRunning())
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemGooglePlay::StartShowLoginUITask: An asynchronous login task is already running."));
		Delegate.ExecuteIfBound(nullptr, PlayerId);
		return;
	}

	if (GameServicesPtr.get() == nullptr)
	{
		// This is likely the first login attempt during this run. Attempt to create the
		// GameServices object, which will automatically start a "silent" login attempt.
		// If that succeeds, there's no need to show the login UI explicitly. If it fails,
		// we'll call ShowAuthorizationUI.
		
		auto TheDelegate = FOnlineAsyncTaskGooglePlayLogin::FOnCompletedDelegate::CreateLambda([this, PlayerId, Delegate]()
		{
			 StartShowLoginUITask_Internal(PlayerId, Delegate);
		});

		CurrentLoginTask = new FOnlineAsyncTaskGooglePlayLogin(this, PlayerId, TheDelegate);
		QueueAsyncTask(CurrentLoginTask);
	}
	else
	{
		// We already have a GameServices object, so we can directly go to ShowAuthorizationUI.
		StartShowLoginUITask_Internal(PlayerId, Delegate);
	}
}

void FOnlineSubsystemGooglePlay::StartLogoutTask(int32 LocalUserNum)
{
	if (CurrentLogoutTask != nullptr)
	{
		UE_LOG(LogOnline, Log, TEXT("FOnlineSubsystemGooglePlay::StartLogoutTask: A logout task is already in progress."));
		IdentityInterface->TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
		return;
	}

	CurrentLogoutTask = new FOnlineAsyncTaskGooglePlayLogout(this, LocalUserNum);
	QueueAsyncTask(CurrentLogoutTask);
}

void FOnlineSubsystemGooglePlay::ProcessGoogleClientConnectResult(bool bInSuccessful, FString AccessToken)
{
	if (CurrentShowLoginUITask != nullptr)
	{
		// Only one login task should be active at a time
		check(CurrentLoginTask == nullptr);

		CurrentShowLoginUITask->ProcessGoogleClientConnectResult(bInSuccessful, AccessToken);
	}
}

void FOnlineSubsystemGooglePlay::StartShowLoginUITask_Internal(int PlayerId, const FOnLoginUIClosedDelegate& Delegate)
{
	check(!AreAnyAsyncLoginTasksRunning());

	CurrentShowLoginUITask = new FOnlineAsyncTaskGooglePlayShowLoginUI(this, PlayerId, Delegate);
	QueueAsyncTask(CurrentShowLoginUITask);
}

void FOnlineSubsystemGooglePlay::QueueAsyncTask(FOnlineAsyncTask* AsyncTask)
{
	check(OnlineAsyncTaskThreadRunnable);
	OnlineAsyncTaskThreadRunnable->AddToInQueue(AsyncTask);
}

void FOnlineSubsystemGooglePlay::OnAuthActionFinished(AuthOperation Op, AuthStatus Status)
{
	if (Op == AuthOperation::SIGN_IN)
	{
		if (CurrentLoginTask != nullptr)
		{
			// Only one login task should be active at a time
			check(CurrentShowLoginUITask == nullptr);

			CurrentLoginTask->OnAuthActionFinished(Op, Status);
		}
		else if(CurrentShowLoginUITask != nullptr)
		{
			// Only one login task should be active at a time
			check(CurrentLoginTask == nullptr);

			CurrentShowLoginUITask->OnAuthActionFinished(Op, Status);
		}
	}
	else if (Op == AuthOperation::SIGN_OUT)
	{
		if (CurrentLogoutTask != nullptr)
		{
			CurrentLogoutTask->OnAuthActionFinished(Op, Status);
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

extern "C" void Java_com_epicgames_ue4_GameActivity_nativeGoogleClientConnectCompleted(JNIEnv* jenv, jobject thiz, jboolean bSuccess, jstring accessToken)
{
	FString AccessToken;
	if (bSuccess)
	{
		const char* charsToken = jenv->GetStringUTFChars(accessToken, 0);
		AccessToken = FString(UTF8_TO_TCHAR(charsToken));
		jenv->ReleaseStringUTFChars(accessToken, charsToken);
	}

	DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.ProcessGoogleClientConnectResult"), STAT_FSimpleDelegateGraphTask_ProcessGoogleClientConnectResult, STATGROUP_TaskGraphTasks);

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
		FSimpleDelegateGraphTask::FDelegate::CreateLambda([=]()
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Google Client connected %s, Access Token: %s\n"), bSuccess ? TEXT("successfully") : TEXT("unsuccessfully"), *AccessToken);
			if (FOnlineSubsystemGooglePlay* const OnlineSub = (FOnlineSubsystemGooglePlay*)IOnlineSubsystem::Get())
			{
				OnlineSub->ProcessGoogleClientConnectResult(bSuccess, AccessToken);
			}
		}),
		GET_STATID(STAT_FSimpleDelegateGraphTask_ProcessGoogleClientConnectResult),
		nullptr,
		ENamedThreads::GameThread
	);
}
