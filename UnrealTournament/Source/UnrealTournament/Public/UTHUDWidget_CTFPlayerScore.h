// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTHUDWidget_DMPlayerScore.h"
#include "UTHUDWidget_CTFPlayerScore.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_CTFPlayerScore : public UUTHUDWidget_DMPlayerScore
{
	GENERATED_UCLASS_BODY()

	virtual void Draw_Implementation(float DeltaTime);
};