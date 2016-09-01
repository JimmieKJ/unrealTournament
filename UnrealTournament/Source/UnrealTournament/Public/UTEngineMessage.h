// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "UTEngineMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTEngineMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	/** Message displayed in message dialog when player pawn fails to spawn because no playerstart was available. */
	UPROPERTY()
		FText FailedPlaceMessage;

	/** Message when player join attempt is refused because the server is at capacity. */
	UPROPERTY()
		FText MaxedOutMessage;

	/** Message when a new player enters the game. */
	UPROPERTY()
		FText EnteredMessage;

	/** Message when a player leaves the game. */
	UPROPERTY()
		FText LeftMessage;

	/** Message when a player changes his name. */
	UPROPERTY()
		FText GlobalNameChange;

	/** Message when a new spectator enters the server (if spectator has a player name). */
	UPROPERTY()
		FText SpecEnteredMessage;

	/** Message when a new player enters the server (if player is unnamed). */
	UPROPERTY()
		FText NewPlayerMessage;

	/** Message when a new spectator enters the server (if spectator is unnamed). */
	UPROPERTY()
		FText NewSpecMessage;

	UPROPERTY()
		FText ServerNotResponding;

	virtual FText GetText(int32 Switch = 0, bool bTargetsPlayerState1 = false, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL) const override;
	virtual FString GetConsoleString(const FClientReceiveData& ClientData, FText LocalMessageText) const override;
};