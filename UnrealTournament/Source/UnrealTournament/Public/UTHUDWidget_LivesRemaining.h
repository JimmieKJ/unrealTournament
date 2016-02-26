// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once 

#include "UTHUDWidget_LivesRemaining.generated.h"

UCLASS()
class UNREALTOURNAMENT_API UUTHUDWidget_LivesRemaining : public UUTHUDWidget
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Draw_Implementation(float DeltaTime);
};