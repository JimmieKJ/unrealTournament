// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"
#include "OnlineSubsystemImpl.h"
#include "OnlineIdentityInterfaceGooglePlay.h"
#include "OnlineAchievementsInterfaceGooglePlay.h"
#include "OnlineLeaderboardInterfaceGooglePlay.h"
#include "OnlineExternalUIInterfaceGooglePlay.h"
#include "UniquePtr.h"

#include "gpg/game_services.h"

class FOnlineAsyncTaskGooglePlayLogin;
class FRunnableThread;

/**
 * OnlineSubsystemGooglePlay - Implementation of the online subsystem for Google Play services
 */
class ONLINESUBSYSTEMGOOGLEPLAY_API FOnlineSubsystemGooglePlay : 
	public FOnlineSubsystemImpl
{
public:
	FOnlineSubsystemGooglePlay();
	virtual ~FOnlineSubsystemGooglePlay() {}

	// Begin IOnlineSubsystem Interface
	virtual IOnlineSessionPtr GetSessionInterface() const override;
	virtual IOnlineFriendsPtr GetFriendsInterface() const override;
	virtual IOnlinePartyPtr GetPartyInterface() const override;
	virtual IOnlineGroupsPtr GetGroupsInterface() const override;
	virtual IOnlineSharedCloudPtr GetSharedCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface() const override;
	virtual IOnlineUserCloudPtr GetUserCloudInterface(const FString& Key) const override;
	virtual IOnlineLeaderboardsPtr GetLeaderboardsInterface() const override;
	virtual IOnlineVoicePtr GetVoiceInterface() const  override;
	virtual IOnlineExternalUIPtr GetExternalUIInterface() const override;
	virtual IOnlineTimePtr GetTimeInterface() const override;
	virtual IOnlineIdentityPtr GetIdentityInterface() const override;
	virtual IOnlineTitleFilePtr GetTitleFileInterface() const override;
	virtual IOnlineEntitlementsPtr GetEntitlementsInterface() const override;
	virtual IOnlineStorePtr GetStoreInterface() const override;
	virtual IOnlineEventsPtr GetEventsInterface() const override { return NULL; }
	virtual IOnlineMessagePtr GetMessageInterface() const override { return NULL; }
	virtual IOnlineSharingPtr GetSharingInterface() const override { return NULL; }
	virtual IOnlineUserPtr GetUserInterface() const override { return NULL; }
	virtual IOnlineAchievementsPtr GetAchievementsInterface() const override;
	virtual IOnlinePresencePtr GetPresenceInterface() const override { return NULL; }
	virtual IOnlineChatPtr GetChatInterface() const override { return NULL; }
	virtual IOnlineTurnBasedPtr GetTurnBasedInterface() const override { return NULL; }

	virtual class UObject* GetNamedInterface(FName InterfaceName) override { return NULL; }
	virtual void SetNamedInterface(FName InterfaceName, class UObject* NewInterface) override {}
	virtual bool IsDedicated() const override { return false; }
	virtual bool IsServer() const override { return true; }
	virtual void SetForceDedicated(bool bForce) override {}
	virtual bool IsLocalPlayer(const FUniqueNetId& UniqueId) const override { return true; }

	virtual bool Init() override;
	virtual bool Shutdown() override;
	virtual FString GetAppId() const override;
	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End IOnlineSubsystem Interface

	virtual bool Tick(float DeltaTime) override;
	
PACKAGE_SCOPE:

	/**
	 * Is Online Subsystem Android available for use
	 * @return true if Android Online Subsystem functionality is available, false otherwise
	 */
	bool IsEnabled();

	/** Return the async task manager owned by this subsystem */
	class FOnlineAsyncTaskManagerGooglePlay* GetAsyncTaskManager() { return OnlineAsyncTaskThreadRunnable.Get(); }

	/**
	 * Add an async task onto the task queue for processing
	 * @param AsyncTask - new heap allocated task to process on the async task thread
	 */
	void QueueAsyncTask(class FOnlineAsyncTask* AsyncTask);

	/** Returns a pointer to the Google API entry point */
	gpg::GameServices* GetGameServices() const { return GameServicesPtr.Get(); }

	/** Utility function, useful for Google APIs that take a std::string but we only have an FString */
	static std::string ConvertFStringToStdString(const FString& InString);

	/** Returns the Google Play-specific version of Identity, useful to avoid unnecessary casting */
	FOnlineIdentityGooglePlayPtr GetIdentityGooglePlay() const { return IdentityInterface; }

	/** Returns the Google Play-specific version of Achievements, useful to avoid unnecessary casting */
	FOnlineAchievementsGooglePlayPtr GetAchievementsGooglePlay() const { return AchievementsInterface; }

	/**
	* Is IAP available for use
	* @return true if IAP should be available, false otherwise
	*/
	bool IsInAppPurchasingEnabled();

private:

	/** Google callback when auth is complete */
	void OnAuthActionFinished(gpg::AuthOperation Op, gpg::AuthStatus Status);

	/** Android callback when an activity is finished */
	void OnActivityResult(JNIEnv *env, jobject thiz, jobject activity, jint requestCode, jint resultCode, jobject data);

	/** Online async task runnable */
	TUniquePtr<class FOnlineAsyncTaskManagerGooglePlay> OnlineAsyncTaskThreadRunnable;

	/** Online async task thread */
	TUniquePtr<FRunnableThread> OnlineAsyncTaskThread;

	/** Interface to the online identity system */
	FOnlineIdentityGooglePlayPtr IdentityInterface;

	FOnlineStoreGooglePlayPtr StoreInterface;

	/** Interface to the online leaderboards */
	FOnlineLeaderboardsGooglePlayPtr LeaderboardsInterface;

	/** Interface to the online achievements */
	FOnlineAchievementsGooglePlayPtr AchievementsInterface;

	/** Interface to the external UI services */
	FOnlineExternalUIGooglePlayPtr ExternalUIInterface;

	/** Pointer to the main entry point for the Google API */
	TUniquePtr<gpg::GameServices> GameServicesPtr;

	/** Track the current login task (if any) so callbacks can notify it */
	FOnlineAsyncTaskGooglePlayLogin* CurrentLoginTask;

	/** Handle of registered delegate for onActivityResult */
	FDelegateHandle OnActivityResultDelegateHandle;
};

typedef TSharedPtr<FOnlineSubsystemGooglePlay, ESPMode::ThreadSafe> FOnlineSubsystemGooglePlayPtr;
