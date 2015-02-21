// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTVictimMessage.generated.h"

UCLASS()
class UUTVictimMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText YouWereKilledByText;

	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const override;
	virtual FLinearColor GetMessageColor(int32 MessageIndex) const override;
};



