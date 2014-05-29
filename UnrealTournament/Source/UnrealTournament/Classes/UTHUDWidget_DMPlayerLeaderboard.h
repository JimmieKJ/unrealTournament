// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once 

/**
 *
 **/

#include "UTHUDWidget_DMPlayerLeaderboard.generated.h"

UCLASS()
class UUTHUDWidget_DMPlayerLeaderboard : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void CalcStanding(int32& Standing, int32& Spread);
	virtual FText GetPlaceSuffix(int32 Standing);
	virtual void Draw(float DeltaTime);

protected:

};
