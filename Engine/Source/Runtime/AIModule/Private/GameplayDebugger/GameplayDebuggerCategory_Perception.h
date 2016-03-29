// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#if WITH_GAMEPLAY_DEBUGGER

#pragma once
#include "GameplayDebuggerCategory.h"

class FGameplayDebuggerCategory_Perception : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Perception();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();
};

#endif // WITH_GAMEPLAY_DEBUGGER
