// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "UTHUDWidgetMessage_GameMessages.generated.h"

UCLASS()
class UUTHUDWidgetMessage_GameMessages : public UUTHUDWidgetMessage 
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool ShouldDraw_Implementation(bool bShowScores) override
	{
		return !bShowScores;
	}

protected:
	float GetTextScale(int32 QueueIndex);
};
