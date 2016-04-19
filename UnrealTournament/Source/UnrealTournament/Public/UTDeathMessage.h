// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "UTDeathMessage.generated.h"

UCLASS(config=game)
class UNREALTOURNAMENT_API UUTDeathMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText GenericKillMessage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText SuicideKillMessage;

	virtual void ClientReceive(const FClientReceiveData& ClientData) const override;
	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const override;
	virtual FLinearColor GetMessageColor_Implementation(int32 MessageIndex) const override;
};



