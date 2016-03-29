// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////
// THIS CLASS IS NOW DEPRECATED AND WILL BE REMOVED IN NEXT VERSION
// Please check GameplayDebugger.h for details.

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

ENUM_RANGE_BY_FIRST_AND_LAST(EAIDebugDrawDataView::Type, EAIDebugDrawDataView::OverHead, EAIDebugDrawDataView::NavMesh)

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

DECLARE_MULTICAST_DELEGATE(FOnCycleDetailsView);
