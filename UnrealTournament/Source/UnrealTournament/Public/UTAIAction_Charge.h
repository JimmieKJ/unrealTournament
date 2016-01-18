// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTAIAction_WaitForMove.h"
#include "UTBot.h"

#include "UTAIAction_Charge.generated.h"

UCLASS(CustomConstructor)
class UNREALTOURNAMENT_API UUTAIAction_Charge : public UUTAIAction_WaitForMove
{
	GENERATED_UCLASS_BODY()

	UUTAIAction_Charge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	{}

	virtual bool Update(float DeltaTime) override
	{
		return (GetEnemy() == NULL || Super::Update(DeltaTime));
	}
};