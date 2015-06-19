// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/ActorChannel.h"

#include "UnitTestActorChannel.generated.h"


/**
 * An actor net channel override, for hooking ReceivedBunch, to aid in detecting/blocking of remote actors, of a specific class
 */
UCLASS(transient)
class UUnitTestActorChannel : public UActorChannel
{
	GENERATED_UCLASS_BODY()

	virtual void ReceivedBunch(FInBunch& Bunch) override;

	virtual void Tick() override;
};

