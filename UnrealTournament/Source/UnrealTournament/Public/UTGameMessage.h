// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "UTGameMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTGameMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText GameBeginsMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText OvertimeMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText SuddenDeathMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText CantBeSpectator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText CantBePlayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText SwitchLevelMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText NoNameChange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText BecameSpectator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText DidntMakeTheCut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText YouAreOnRed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText YouAreOnBlue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText Coronation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText GameChanger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText KickVote;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText NotEnoughMoney;

	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const override;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject) const override;
	virtual FLinearColor GetMessageColor(int32 MessageIndex) const override;
	virtual bool UseLargeFont(int32 MessageIndex) const override;
	virtual float GetScaleInSize(int32 MessageIndex) const override;
};

