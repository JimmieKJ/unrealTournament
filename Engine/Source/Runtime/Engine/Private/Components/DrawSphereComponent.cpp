// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Components/DrawSphereComponent.h"
#if WITH_EDITOR
#include "ShowFlags.h"
#include "ConvexVolume.h"
#endif

UDrawSphereComponent::UDrawSphereComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bHiddenInGame = true;
	bUseEditorCompositing = true;
	bGenerateOverlapEvents = false;
}

#if WITH_EDITOR
bool UDrawSphereComponent::ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	// Draw sphere components not treated as 'selectable' in editor
	return false;
}

bool UDrawSphereComponent::ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const
{
	// Draw sphere components not treated as 'selectable' in editor
	return false;
}
#endif
