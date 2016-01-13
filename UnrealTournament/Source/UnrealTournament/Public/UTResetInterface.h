// implement this interface for Actors that should handle game mode resets (halftime in CTF, role swap in Assault, etc)
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTResetInterface.generated.h"

UINTERFACE(MinimalAPI)
class UUTResetInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class UNREALTOURNAMENT_API IUTResetInterface
{
	GENERATED_IINTERFACE_BODY()

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Game)
	void Reset();
};
