// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTGameEngine.generated.h"

UCLASS()
class UUTGameEngine : public UGameEngine
{
	GENERATED_UCLASS_BODY()

	/** default screenshot used for levels when none provided in the level itself */
	UPROPERTY()
	UTexture2D* DefaultLevelScreenshot;

	UPROPERTY()
	FText ReadEULACaption;
	UPROPERTY()
	FText ReadEULAText;

	/** used to display EULA info on first run */
	UPROPERTY(globalconfig)
	bool bFirstRun;

	UPROPERTY(config)
	int32 ParallelRendererProcessorRequirement;

	/** UT specific networking version.  Must match for net compatibility. */
	UPROPERTY()
	int32 GameNetworkVersion;

	//==================================
	// Frame Rate Smoothing
	
	/** Current smoothed delta time. */
	UPROPERTY()
	float SmoothedDeltaTime;

	/** Frame time (in seconds) longer than this is considered a hitch. */
	UPROPERTY(config)
	float HitchTimeThreshold;

	/** Frame time longer than SmoothedDeltaTime*HitchScaleThreshold is considered a hitch. */
	UPROPERTY(config)
	float HitchScaleThreshold;

	/** How fast to smooth up from a hitch frame. */
	UPROPERTY(config)
	float HitchSmoothingRate;

	/** How fast to smooth between normal frames. */
	UPROPERTY(config)
	float NormalSmoothingRate;

	/** Never return a smoothed time larger than this. */
	UPROPERTY(config)
	float MaximumSmoothedTime;
	
	//==================================

	/* Set true to allow clients to toggle netprofiling using the NP console command. @TODO FIXMESTEVE temp until we have adminlogin/admin server console command executing */
	UPROPERTY(config)
	bool bAllowClientNetProfile;

	/* Frame rate cap */
	UPROPERTY(config)
	float FrameRateCap;

	UPROPERTY(config)
	FString RconPassword;

	/** Max prediction ping (used when negotiating with clients) */
	float ServerMaxPredictionPing;

	/** set to process ID of owning game client when running a "listen" server (which is really dedicated + client on same machine) */
	uint32 OwningProcessID;

	TMap<FString, FString> DownloadedContentChecksums;
	TMap<FString, FString> LocalContentChecksums;
	TMap<FString, FString> CloudContentChecksums;

	FString ContentDownloadCloudId;
	TMap<FString, FString> FilesToDownload;

	virtual void Init(IEngineLoop* InEngineLoop);
	virtual void PreExit();
	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out = *GLog) override;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	virtual float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing) const override;
	virtual void UpdateRunningAverageDeltaTime(float DeltaTime, bool bAllowFrameRateSmoothing = true) override;
	virtual void LoadDownloadedAssetRegistries();

	virtual EBrowseReturnVal::Type Browse(FWorldContext& WorldContext, FURL URL, FString& Error) override;

	FString MD5Sum(const TArray<uint8>& Data);
	bool IsCloudAndLocalContentInSync();

	virtual void SetupLoadingScreen();
#define UT_LOADING_SCREEN_HOOK SetupLoadingScreen();
#if CPP
#include "UTLoadMap.h"
#endif
	UT_LOADMAP_DEFINITION()
#undef UT_LOADING_SCREEN_HOOK

	virtual void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString) override;

	/** load the level summary out of a map package */
	static class UUTLevelSummary* LoadLevelSummary(const FString& MapName);

protected:
	virtual bool ShouldShutdownWorldNetDriver() override;
	void OnLoadingMoviePlaybackFinished();
	void PromptForEULAAcceptance();
};

