// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayDebuggingTypes.generated.h"

UENUM()
namespace EAIDebugDrawDataView
{
	enum Type
	{
		Empty,
		OverHead,
		Basic,
		BehaviorTree,
		EQS,
		Perception,
		GameView1,
		GameView2,
		GameView3,
		GameView4,
		GameView5,
		NavMesh,
		MAX UMETA(Hidden)
	};
}

struct GAMEPLAYDEBUGGER_API FGameplayDebuggerSettings
{
	explicit FGameplayDebuggerSettings(uint32& Flags)
		: DebuggerShowFlags(Flags)
	{

	}
	void SetFlag(uint8 Flag)
	{
		DebuggerShowFlags |= (1 << Flag);
	}

	void ClearFlag(uint8 Flag)
	{
		DebuggerShowFlags &= ~(1 << Flag);
	}

	bool CheckFlag(uint8 Flag)
	{
		return (DebuggerShowFlags & (1 << Flag)) != 0;
	}

	uint32 &DebuggerShowFlags;
};

GAMEPLAYDEBUGGER_API
FGameplayDebuggerSettings  GameplayDebuggerSettings(class AGameplayDebuggingReplicator* Replicator = NULL);

DECLARE_MULTICAST_DELEGATE(FOnChangeEQSQuery);

DECLARE_LOG_CATEGORY_EXTERN(LogGameplayDebugger, Warning, All);
