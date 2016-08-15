// implement this interface for Actors that need to be notified when Intermission begins in CTF
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTIntermissionBeginInterface.generated.h"

UINTERFACE(MinimalAPI)
class UUTIntermissionBeginInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class UNREALTOURNAMENT_API IUTIntermissionBeginInterface
{
	GENERATED_IINTERFACE_BODY()

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = Game)
	void IntermissionBegin();
};