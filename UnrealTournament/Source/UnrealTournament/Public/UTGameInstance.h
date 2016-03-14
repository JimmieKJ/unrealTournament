// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "../../Engine/Source/Runtime/PerfCounters/Private/PerfCounters.h"
#include "OnlineSessionInterface.h"
#include "UTPlaylistManager.h"
#include "UTGameInstance.generated.h"

class UUTMatchmaking;
class UUTParty;

enum EUTNetControlMessage
{
	UNMT_Redirect = 0, // required download from redirect
};

UCLASS()
class UNREALTOURNAMENT_API UUTGameInstance : public UGameInstance
{
	GENERATED_UCLASS_BODY()
	
	virtual void Init() override;
	virtual void Shutdown() override;
	virtual bool PerfExecCmd(const FString& ExecCmd, FOutputDevice& Ar);
	virtual void StartGameInstance() override;

	virtual void StartRecordingReplay(const FString& Name, const FString& FriendlyName, const TArray<FString>& AdditionalOptions = TArray<FString>()) override;
	virtual void PlayReplay(const FString& Name, UWorld* WorldOverride = nullptr, const TArray<FString>& AdditionalOptions = TArray<FString>()) override;

	virtual void HandleGameNetControlMessage(class UNetConnection* Connection, uint8 MessageByte, const FString& MessageStr) override;

	bool IsAutoDownloadingContent();

	virtual bool DelayPendingNetGameTravel() override
	{
		return IsAutoDownloadingContent();
	}

	/** starts download for pak file from redirect if needed
	 * @return whether download was required */
	virtual bool StartRedirectDownload(const FString& PakName, const FString& URL, const FString& Checksum);

	inline void SetLastTriedDemo(const FString& NewName)
	{
		if (NewName != LastTriedDemo)
		{
			LastTriedDemo = NewName;
			bRetriedDemoAfterRedirects = false;
		}
	}
	inline FString GetLastTriedDemo() const
	{
		return LastTriedDemo;
	}


inline void InitPerfCounters()
{
#if !UE_SERVER
	
	IPerfCountersModule& PerfCountersModule = FModuleManager::LoadModuleChecked<IPerfCountersModule>("PerfCounters");
	IPerfCounters* PerfCounters = PerfCountersModule.CreatePerformanceCounters();
	
	if (PerfCounters != nullptr)
	{
		// Not exactly full version string, but the build number
		//UE_LOG(UT,Log,TEXT("GEngineNetVersion %i"),GEngineNetVersion);
		PerfCounters->Set(TEXT("BuildVersion"), GEngineNetVersion);
	}
	else
	{
		UE_LOG(LogInit, Warning, TEXT("Could not initialize performance counters."));
	}
#endif
}

protected:
	virtual void DeferredStartGameInstance();

	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld) override;

	virtual void HandleDemoPlaybackFailure( EDemoPlayFailure::Type FailureType, const FString& ErrorString ) override;

	virtual void RedirectResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	TArray<TWeakPtr<class SUTRedirectDialog>> ActiveRedirectDialogs;

	// in order to handle demo redirects, we have to cancel the demo, download, then retry
	// this tracks the demo we need to retry
	FString LastTriedDemo;
	bool bRetriedDemoAfterRedirects;


	/** Matchmaking singleton */
	UPROPERTY(Transient)
	UUTMatchmaking* Matchmaking;

	/** Parties singleton */
	UPROPERTY(Transient)
	UUTParty* Party;

	UPROPERTY(Transient)
	UUTPlaylistManager* PlaylistManager;
	
	/** Timer waiting to call SafeSessionDelete again because session was in a creating/ending state */
	FTimerHandle SafeSessionDeleteTimerHandle;

	/** Array of destroy session delegates gathered while the session was in a destroying state */
	TMap<FName, TArray<FOnDestroySessionCompleteDelegate>> PendingDeletionDelegates;
	FDelegateHandle DeleteSessionDelegateHandle;
	
	/**
	 * Internal handler for SafeSessionDelete calls and then fire the user delegate on success
	 *
	 * @param SessionName name of session
	 * @param bWasSuccessful true if successful, false otherwise
	 * @param DestroySessionComplete delegate to fire when the entire operation is done, successful or not
	 */
	void OnDeleteSessionComplete(FName SessionName, bool bWasSuccessful, FOnDestroySessionCompleteDelegate DestroySessionComplete);

public:
	
	/** Returns the game instance corresponding to the context object, or NULL if none */
	static UUTGameInstance* Get(UObject* ContextObject);

	/**
	 * Matchmaking
	 */
	UUTMatchmaking* GetMatchmaking() const;
	
	/**
	 * Parties
	 */
	UUTParty* GetParties() const;

	UUTPlaylistManager* GetPlaylistManager() const;

	bool IsInSession(const FUniqueNetId& SessionId) const;

	/**
	 * Safe delete mechanism to make sure we aren't deleting a session too soon after its creation
	 *
	 * @param SessionName the name of the session this callback is for
	 * @param DestroySessionComplete delegate to call on completion, in all cases
	 */
	void SafeSessionDelete(FName SessionName, FOnDestroySessionCompleteDelegate DestroySessionComplete);
	
	/**
	 * Helper function for traveling to a session that has already been joined via the online platform
	 * Grabs the URL from the session info and travels
	 *
	 * @param ControllerId controller initiating the request
	 * @param InSessionName name of session to travel to 
	 *
	 * @return true if able or attempting to travel, false otherwise
	 */
	bool ClientTravelToSession(int32 ControllerId, FName InSessionName);
};

