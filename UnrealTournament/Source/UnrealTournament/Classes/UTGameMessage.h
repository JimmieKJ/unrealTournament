// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once


#include "UTGameMessage.generated.h"

UCLASS()
class UUTGameMessage : public UUTLocalMessage
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

	virtual FText GetText(int32 Switch,bool bTargetsPlayerState1,class APlayerState* RelatedPlayerState_1,class APlayerState* RelatedPlayerState_2,class UObject* OptionalObject) const OVERRIDE;
};

