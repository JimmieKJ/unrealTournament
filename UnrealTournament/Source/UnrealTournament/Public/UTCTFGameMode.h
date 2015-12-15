// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTCTFBaseGame.h"
#include "UTCTFGameMode.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFGameMode : public AUTCTFBaseGame
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=CTF)
	int32 HalftimeDuration;

	/**Holds the amount of time to give a flag carrier who has the flag out going in to half-time*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CTF)
	int32 AdvantageDuration;

	UFUNCTION(exec)
		void SetRemainingTime(int32 RemainingSeconds);

	virtual void InitGame( const FString& MapName, const FString& Options, FString& ErrorMessage ) override;
	virtual void ScoreObject_Implementation(AUTCarriedObject* GameObject, AUTCharacter* HolderPawn, AUTPlayerState* Holder, FName Reason) override;
	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	virtual void CheckGameTime();
	virtual void DefaultTimer() override;

	virtual void CallMatchStateChangeNotify() override;
	virtual float GetTravelDelay() override;

	virtual void HandleEnteringHalftime();
	virtual void HandleHalftime();
	virtual void HandleExitingHalftime();
	virtual void HandleEnteringOvertime();
	virtual void HandleMatchInOvertime() override;

	virtual bool PlayerCanRestart_Implementation(APlayerController* Player);

	void BuildServerResponseRules(FString& OutRules);

	virtual void GetGood() override;

protected:

	virtual void HandleMatchHasStarted();

	UFUNCTION()
	virtual void HalftimeIsOver();

	// returns the team index of a team with advatage or < 0 if no team has one
	virtual uint8 TeamWithAdvantage();

	// Look to see if the team that had advantage still has it
	virtual bool CheckAdvantage();

	
	int32 RemainingAdvantageTime;

	virtual void EndOfHalf();
};


