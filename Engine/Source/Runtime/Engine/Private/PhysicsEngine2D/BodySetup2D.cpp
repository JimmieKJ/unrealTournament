// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "PhysicsEngine/BodySetup2D.h"

//////////////////////////////////////////////////////////////////////////
// UBodySetup2D

UBodySetup2D::UBodySetup2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


#if WITH_EDITOR

void UBodySetup2D::InvalidatePhysicsData()
{
	//ClearPhysicsMeshes();
	//BodySetupGuid = FGuid::NewGuid(); // change the guid
	//CookedFormatData.FlushData();
}

#endif

void UBodySetup2D::CreatePhysicsMeshes()
{
}

float UBodySetup2D::GetVolume(const FVector& Scale) const
{
	const float FictionalThickness = 10.0f; //@TODO: PAPER2D: Make tweakable or read from the sprite
	return AggGeom2D.GetArea(Scale) * FictionalThickness;
}
