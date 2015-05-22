// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "UTHUDWidgetMessage_GameMessages.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidgetMessage_GameMessages : public UUTHUDWidgetMessage
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool ShouldDraw_Implementation(bool bShowScores) override
	{
		return !bShowScores;
	}
};
