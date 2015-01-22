// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PhysicsEngine/AggregateGeometry2D.h"
#include "PhysicsEngine/BodySetup.h"
#include "BodySetup2D.generated.h"

UCLASS()
class ENGINE_API UBodySetup2D : public UBodySetup
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FAggregateGeometry2D AggGeom2D;

	// UBodySetup interface
	virtual void CreatePhysicsMeshes() override;
	virtual float GetVolume(const FVector& Scale) const override;
	// End of UBodySetup interface

#if WITH_EDITOR
	// UBodySetup interface
	virtual void InvalidatePhysicsData() override;
	// End of UBodySetup interface
#endif
};
