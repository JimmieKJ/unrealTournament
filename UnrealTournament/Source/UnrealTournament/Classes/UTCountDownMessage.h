// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTCountDownMessage.generated.h"

UCLASS()
class UUTCountDownMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Message")
	FText CountDownText;

	virtual void GetArgs(FFormatNamedArguments& Args, int32 Switch = 0, bool bTargetsPlayerState1 = false,class APlayerState* RelatedPlayerState_1 = NULL,class APlayerState* RelatedPlayerState_2 = NULL,class UObject* OptionalObject = NULL) const;
	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const OVERRIDE;
};

