// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "Paper2DPrivatePCH.h"
#include "PaperBatchSceneProxy.h"
#include "PaperBatchComponent.h"

//////////////////////////////////////////////////////////////////////////
// UPaperBatchComponent

UPaperBatchComponent::UPaperBatchComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bAutoActivate = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bCanEverTick = true;
}

FPrimitiveSceneProxy* UPaperBatchComponent::CreateSceneProxy()
{
	FPaperBatchSceneProxy* NewProxy = new FPaperBatchSceneProxy(this);
	return NewProxy;
}

FBoxSphereBounds UPaperBatchComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Always visible
	FBoxSphereBounds Bounds(FVector::ZeroVector, FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX), HALF_WORLD_MAX);
	return Bounds;
}

void UPaperBatchComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Draw all the debug rendering for the 2D physics scene
	//if (View->Family->EngineShowFlags.Collision && AllowDebugViewmodes())
	{
	//	FPhysicsIntegration2D::DrawDebugPhysics(GetWorld(), PDI, View); //@TODO: GWorld is worse than the disease
	}

	
}
