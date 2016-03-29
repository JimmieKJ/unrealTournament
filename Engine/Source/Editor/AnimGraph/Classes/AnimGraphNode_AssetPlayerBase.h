// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_AssetPlayerBase.generated.h"

class UAnimSequenceBase;

/** Helper / intermediate for asset player graphical nodes */
UCLASS(Abstract, MinimalAPI)
class UAnimGraphNode_AssetPlayerBase : public UAnimGraphNode_Base
{
	GENERATED_BODY()
public:
	// Sync group settings for this player.  Sync groups keep related animations with different lengths synchronized.
	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimationGroupReference SyncGroup;
};