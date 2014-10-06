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

	/* Set true to allow clients to toggle netprofiling using the NP console command. @TODO FIXMESTEVE temp until we have adminlogin/admin server console command executing */
	UPROPERTY(config)
	bool bAllowClientNetProfile;

	// Maximum tick rate for a client
	UPROPERTY()
	float CurrentMaxTickRate;

	// How many frames in a row we've surpassed maximum tick rate
	UPROPERTY()
	int32 MadeFrames;

	// How many times we've raised maximum tick rate in a row
	UPROPERTY()
	int32 MadeFramesStreak;

	// How many times we've been slower than maximum tick rate since the last time it was raised
	UPROPERTY()
	int32 MissedFrames;

	/* How much to reduce maximum frame rate for missing frame rate too often */
	UPROPERTY(config)
	float MissedFramePenalty;
	
	/* How much to raise frame rate by when surpassing maximum frame rate */
	UPROPERTY(config)
	float MadeFrameBonus;

	/* How many frames must be missed before considering applying a penalty */
	UPROPERTY(config)
	int32 MissedFrameThreshold;

	/* Starting point for a sliding scale of how many seconds of frames must be consistently good before raising the cap */
	UPROPERTY(config)
	float MadeFrameStartingThreshold;

	/* Ending point for a sliding scale of how many seconds of frames must be consistently good before raising the cap */
	UPROPERTY(config)
	float MadeFrameMinimumThreshold;

	/* Frame rate cap */
	UPROPERTY(config)
	float FrameRateCap;

	/* Frame rate minimum for smoothing to kick in */
	UPROPERTY(config)
	float FrameRateMinimum;

	/** Max prediction ping (used when negotiating with clients) */
	float ServerMaxPredictionPing;

	float RunningAverageDeltaTime;
	float SmoothedFrameRate;

	virtual void Init(IEngineLoop* InEngineLoop);
	virtual void PreExit();
	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out = *GLog) override;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	virtual float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing) override;

	void SmoothFrameRate(float DeltaTime);

	UT_LOADMAP_DEFINITION()
};