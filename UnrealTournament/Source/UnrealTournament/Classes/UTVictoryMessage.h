// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTVictoryMessage.generated.h"

UCLASS()
class UUTVictoryMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText YouHaveWonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText YouHaveLostText;

	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const OVERRIDE;
};

