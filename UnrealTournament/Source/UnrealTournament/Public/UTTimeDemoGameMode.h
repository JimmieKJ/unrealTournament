// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTTimeDemoGameMode.generated.h"

UCLASS(Config = Game, Abstract)
class UNREALTOURNAMENT_API AUTTimeDemoGameMode : public AUTGameMode, public FSelfRegisteringExec
{
	GENERATED_BODY()
	
public:
	AUTTimeDemoGameMode(const class FObjectInitializer& ObjectInitializer);

	virtual bool ReadyToStartMatch_Implementation() override
	{
		return false;
	}

	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	UFUNCTION(BlueprintImplementableEvent, Category = "TimeDemo")
	void StartTimeDemo();

	UFUNCTION(BlueprintImplementableEvent, Category = "TimeDemo")
	void StopTimeDemo();

	UPROPERTY(EditDefaultsOnly)
	TArray<FString> CommandsToRunAtStart;

	UPROPERTY(EditDefaultsOnly)
	TArray<FString> CommandsToRunAtStop;
	
	virtual void Tick(float DeltaTime) override;

	float TimeLeft;
	bool bStartedDemo;

	virtual void ExecuteStartTimeDemoCommands(UWorld* InWorld);
	virtual void ExecuteStopTimeDemoCommands(UWorld* InWorld);
};