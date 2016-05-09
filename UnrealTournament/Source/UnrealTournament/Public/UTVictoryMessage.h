// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAnnouncer.h"

#include "UTVictoryMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTVictoryMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText YouHaveWonText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText YouHaveLostText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText RedTeamWinsText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
	FText BlueTeamWinsText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText RedTeamWinsSecondaryText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Message)
		FText BlueTeamWinsSecondaryText;

	virtual FName GetAnnouncementName_Implementation(int32 Switch, const UObject* OptionalObject, const class APlayerState* RelatedPlayerState_1, const class APlayerState* RelatedPlayerState_2) const override;
	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const override;
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
	virtual void ClientReceive(const FClientReceiveData& ClientData) const override;
};

