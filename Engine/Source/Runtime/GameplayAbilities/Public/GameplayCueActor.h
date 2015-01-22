// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffect.h"
#include "GameplayCueInterface.h"
#include "GameplayCueActor.generated.h"

/**
 *	This is meant to be a basic implementation of an actor is spawned from a GameplayCue event. It implements the cue interface so events can be forwarded to it as well.
 *	It provides basic functionality like attachment.
 *	
 *	Individual projects will most likely want their own subclass (or a completely different GameplayCue handling system all together)
 */


UCLASS(BlueprintType, notplaceable)
class GAMEPLAYABILITIES_API AGameplayCueActor : public AActor, public IGameplayCueInterface
{
	// Defines how a GameplayCue is translated into viewable components
	GENERATED_UCLASS_BODY()

	virtual void BeginPlay() override;

	/** Whether this actor should be attached to its owner on spawn (or otherwise persist in the world on its own) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameplayCueActor", meta=(ExposeOnSpawn=true))
	bool	AttachToOwner;

	/** If attaching, an optional socket name to attach to */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameplayCueActor", meta=(ExposeOnSpawn=true))
	FName	AttachSocketName;

	/** Whether this actor should be destroyed when the originating GameplayCue is removed (for 'WhileAdded' Cues only) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GameplayCueActor", meta=(ExposeOnSpawn=true))
	bool	DestroyWhenCueEnds;
};

