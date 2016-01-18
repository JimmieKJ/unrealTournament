// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPluginPrivatePCH.h"
#include "GameplayDebuggerReplicator.h"

AGameplayDebuggerServerHelper::AGameplayDebuggerServerHelper(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
#if ENABLED_GAMEPLAY_DEBUGGER
	const bool bCanTick = true;
#else
	const bool bCanTick = false;
#endif

	PrimaryActorTick.bCanEverTick = bCanTick;
	PrimaryActorTick.bAllowTickOnDedicatedServer = bCanTick;
	PrimaryActorTick.bTickEvenWhenPaused = bCanTick;
	PrimaryActorTick.bStartWithTickEnabled = bCanTick;
	PrimaryActorTick.TickInterval = 2;

#if WITH_EDITOR
	SetIsTemporarilyHiddenInEditor(true);
#endif

#if WITH_EDITORONLY_DATA
	bHiddenEdLevel = true;
	bHiddenEdLayer = true;
	bHiddenEd = true;
	bEditable = false;
#endif

}

void AGameplayDebuggerServerHelper::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

#if ENABLED_GAMEPLAY_DEBUGGER
	const UWorld* World = GetWorld();
	if (!World || World->IsGameWorld() == false)
	{
		// return without world
		return;
	}

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = *Iterator;
		if (PC)
		{
			IGameplayDebuggerPlugin& Debugger = IGameplayDebuggerPlugin::Get();
			Debugger.CreateGameplayDebuggerForPlayerController(PC);
			PrimaryActorTick.TickInterval = 5;
		}
	}
#endif
}
