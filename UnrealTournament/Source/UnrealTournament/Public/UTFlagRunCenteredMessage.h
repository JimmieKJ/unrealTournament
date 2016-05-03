// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTFlagRunCenteredMessage.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTFlagRunCenteredMessage : public UUTLocalMessage
{
	GENERATED_UCLASS_BODY()
		UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Message)
		FText ReturnMessage;
};

