// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "../../Engine/Source/Runtime/PerfCounters/Private/PerfCounters.h"
#include "OnlineSessionInterface.h"
#include "UTPlaylistManager.h"
#include "UTATypes.h"
#if !UE_SERVER
#include "MoviePlayer.h"
#endif

#include "NetworkVersion.h"
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

	// If set, this movie will be the first movie to play in the loading sequence.  It will be cleared whenever it's used.
	FString LoadingMovieToPlay;

	// The text to display at the top of the screen during the loading process.  Leave empty for no text
	FText LevelLoadText;

	// If true, we will ingore the level load and not display anything.  This is to keep the loading screen from appearing with the killcam.
	bool bIgnoreLevelLoad;

	virtual bool DelayPendingNetGameTravel() override
	{
		return IsAutoDownloadingContent();
	}

	/** starts download for pak file from redirect if needed
	 * @return whether download was required */
	
	virtual bool RedirectDownload(const FString& PakName, const FString& URL, const FString& Checksum);

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
	if (!bDisablePerformanceCounters)
	{
		IPerfCountersModule& PerfCountersModule = FModuleManager::LoadModuleChecked<IPerfCountersModule>("PerfCounters");
		IPerfCounters* PerfCounters = PerfCountersModule.CreatePerformanceCounters();
	
		if (PerfCounters != nullptr)
		{
			// Not exactly full version string, but the build number
			//UE_LOG(UT,Log,TEXT("GEngineNetVersion %i"),GEngineNetVersion);
			PerfCounters->Set(TEXT("BuildVersion"), FNetworkVersion::GetNetworkCompatibleChangelist());
		}
		else
		{
			UE_LOG(LogInit, Warning, TEXT("Could not initialize performance counters."));
		}
	}
}

protected:
	virtual void DeferredStartGameInstance();

	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld) override;

	virtual void HandleDemoPlaybackFailure( EDemoPlayFailure::Type FailureType, const FString& ErrorString ) override;

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

	UFUNCTION()
	virtual void BeginLevelLoading(const FString& LevelName);
	UFUNCTION()
	virtual void EndLevelLoading();

	bool bLevelIsLoading;

	UPROPERTY()
	TArray<FMapVignetteInfo> MovieVignettes;
	int32 VignetteIndex;

#if !UE_SERVER
	void OnMoviePlaybackFinished();

public:
	// Play a loading movie.  This version will auto-create the "Press FIRE to continue" message and manage if it should be displayed
	virtual void PlayLoadingMovies(bool bStopWhenLoadingIsComnpleted);

	// Stops a movie from playing
	virtual void StopMovie();

	// returns true if a movie is currently being played
	virtual bool IsMoviePlaying();

	// Plays a full screen movie  
	virtual void PlayMovie(const FString& MoviePlayList, TSharedPtr<SWidget> SlateOverlayWidget, bool bSkippable, bool bAutoComplete, TEnumAsByte<EMoviePlaybackType> PlaybackType, bool bForce);


protected:

	TSharedPtr<SOverlay> LoadingMovieOverlay;

	// Will be true if we should show the community badge on this vignette
	bool bShowCommunityBadge;

	// Will hold the friendly name (if possible) of the map you are about to load
	FString LoadingMapFriendlyName;
	
	// Create the loading movie overlay
	virtual void CreateLoadingMovieOverlay();
	virtual EVisibility GetLevelLoadThrobberVisibility() const;
	virtual FText GetLevelLoadText() const;
	virtual FText GetVignetteText() const;
	virtual EVisibility GetLevelLoadTextVisibility() const;
	virtual EVisibility GetCommunityVisibility() const;
	virtual EVisibility GetEpicLogoVisibility() const;
	virtual EVisibility GetVignetteVisibility() const;
#endif

	UPROPERTY()
	bool bDisablePerformanceCounters;

	void OnMovieClipFinished(const FString& ClipName);

public:
	virtual int32 GetBotSkillForTeamElo(int32 TeamElo);

	/** If true, the loading movie will suppress the loading text.  Thnis is used for tutorial movies */
	bool bSuppressLoadingText;
};

