// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
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
		FText YouAreOn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText RedTeamName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText BlueTeamName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText KickVote;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
	FText NotEnoughMoney;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText PotentialSpeedHack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText OnDeck;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Message")
		FText WeaponLocked;

	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const override;
	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
	virtual float GetScaleInSize_Implementation(int32 MessageIndex) const override;
	virtual float GetLifeTime(int32 Switch) const override;
	virtual void GetEmphasisText(FText& PrefixText, FText& EmphasisText, FText& PostfixText, FLinearColor& EmphasisColor, int32 Switch, class APlayerState* RelatedPlayerState_1, class APlayerState* RelatedPlayerState_2, class UObject* OptionalObject) const override;
	virtual int32 GetFontSizeIndex(int32 MessageIndex) const override;
};

