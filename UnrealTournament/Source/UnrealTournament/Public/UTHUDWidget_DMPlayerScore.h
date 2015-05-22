// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_DMPlayerScore.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_DMPlayerScore : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	UTexture* HudTexture;

	virtual void Draw_Implementation(float DeltaTime);
protected:
	int32 LastScore;
	float ScoreFlashOpacity;
};
