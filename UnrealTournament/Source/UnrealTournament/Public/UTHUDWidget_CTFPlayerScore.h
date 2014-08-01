// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_DMPlayerScore.h"
#include "UTHUDWidget_CTFPlayerScore.generated.h"

UCLASS()
class UUTHUDWidget_CTFPlayerScore : public UUTHUDWidget_DMPlayerScore
{
	GENERATED_UCLASS_BODY()

	virtual void Draw_Implementation(float DeltaTime);

	virtual bool ShouldDraw_Implementation(bool bShowScores)
	{
		return true;
	}

};