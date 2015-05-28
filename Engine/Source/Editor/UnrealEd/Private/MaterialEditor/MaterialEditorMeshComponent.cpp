// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "MaterialEditor/MaterialEditorMeshComponent.h"
#include "PhysicsEngine/BodySetup.h"

FBoxSphereBounds UMaterialEditorMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (StaticMesh)
	{
		// Graphics bounds.
		FBoxSphereBounds NewBounds = StaticMesh->GetBounds().TransformBy(LocalToWorld);

		// Add bounds of collision geometry (if present).
		if (StaticMesh->BodySetup)
		{
			// Use more accurate but expensive bounds function for the material editor only
			FBoxSphereBounds AggGeomBounds;
			StaticMesh->BodySetup->AggGeom.CalcBoxSphereBounds(AggGeomBounds, LocalToWorld);
			if (AggGeomBounds.SphereRadius != 0.f)
			{
				NewBounds = Union(NewBounds,AggGeomBounds);
			}
		}

		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f);
	}
}