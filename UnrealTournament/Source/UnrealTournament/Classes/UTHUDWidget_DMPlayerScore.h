// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_DMPlayerScore.generated.h"

UCLASS()
class UUTHUDWidget_DMPlayerScore : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY()
	UTexture* HudTexture;

	virtual void Draw(float DeltaTime);
protected:

private:
	int32 LastScore;
	float ScoreFlashOpacity;
};
