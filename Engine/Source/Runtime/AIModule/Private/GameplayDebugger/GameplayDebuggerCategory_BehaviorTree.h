// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategory.h"
#endif

class AActor;
class APlayerController;

#if WITH_GAMEPLAY_DEBUGGER

class FGameplayDebuggerCategory_BehaviorTree : public FGameplayDebuggerCategory
{
public:
	FGameplayDebuggerCategory_BehaviorTree();

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance();

protected:
	struct FRepData
	{
		FString CompName;
		FString TreeDesc;
		FString BlackboardDesc;

		void Serialize(FArchive& Ar);
	};
	FRepData DataPack;
};

#endif // WITH_GAMEPLAY_DEBUGGER
