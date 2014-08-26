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

	// Maximum tick rate for a client
	UPROPERTY()
	float CurrentMaxTickRate;

	// How many frames in a row we've surpassed maximum tick rate
	UPROPERTY()
	int32 MadeFrames;

	// How many times we've raised maximum tick rate in a row
	UPROPERTY()
	int32 MadeFramesStreak;

	// How many times we've been slower than maxium tick rate since the last time it was raised
	UPROPERTY()
	int32 MissedFrames;

	virtual void Init(IEngineLoop* InEngineLoop);
	virtual void PreExit();
	virtual bool HandleOpenCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld *InWorld) override;
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Out = *GLog) override;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	virtual float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing) override;

	UT_LOADMAP_DEFINITION()
};