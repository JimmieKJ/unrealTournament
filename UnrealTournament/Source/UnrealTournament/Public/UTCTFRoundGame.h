// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UTCTFGameState.h"
#include "UTCTFScoring.h"
#include "UTCTFBaseGame.h"
#include "UTCTFRoundGame.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTCTFRoundGame : public AUTCTFBaseGame
{
	GENERATED_UCLASS_BODY()

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual bool CheckScore_Implementation(AUTPlayerState* Scorer);
	void BuildServerResponseRules(FString& OutRules);
	virtual void HandleFlagCapture(AUTPlayerState* Holder) override;
	virtual void HandleExitingIntermission() override;
	virtual void InitGameState() override;
	virtual int32 IntermissionTeamToView(AUTPlayerController* PC) override;
};