// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLoadMap.h"

#include "UTGameEngine.generated.h"

UCLASS()
class UUTGameEngine : public UGameEngine
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FText ReadEULACaption;
	UPROPERTY()
	FText ReadEULAText;

	/** used to display EULA info on first run */
	UPROPERTY(globalconfig)
	bool bFirstRun;

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

	virtual void Init(IEngineLoop* InEngineLoop);
	virtual void PreExit();
	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out = *GLog) override;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	virtual float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing) const override;
	virtual void UpdateRunningAverageDeltaTime(float DeltaTime, bool bAllowFrameRateSmoothing = true) override;
	virtual void LoadDownloadedPakFiles();

	UT_LOADMAP_DEFINITION()
};