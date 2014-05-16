// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "UTKillerMessage.generated.h"

UCLASS()
class UUTKillerMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText YouKilledText;

	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const;
};



