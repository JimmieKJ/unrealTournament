// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategory.h"
#endif

class AActor;
class APlayerController;

#if WITH_GAMEPLAY_DEBUGGER


class FGameplayDebuggerCategory_Perception : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_Perception();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();
};

#endif // WITH_GAMEPLAY_DEBUGGER
