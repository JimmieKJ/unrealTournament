// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLocalPlayer.generated.h"

/** Defines the current state of the game. */


UCLASS()
class UUTLocalPlayer : public ULocalPlayer
{
	GENERATED_UCLASS_BODY()

public:
	virtual FString GetNickname() const;
protected:
};




