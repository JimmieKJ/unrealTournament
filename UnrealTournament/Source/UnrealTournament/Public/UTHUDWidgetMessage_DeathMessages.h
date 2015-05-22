// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "UTHUDWidgetMessage_DeathMessages.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetMessage_DeathMessages : public UUTHUDWidgetMessage
{
	GENERATED_UCLASS_BODY()

	virtual bool ShouldDraw_Implementation(bool bShowScores) override
	{
		return !bShowScores;
	}

protected:
	virtual void DrawMessages(float DeltaTime);
};
