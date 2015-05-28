// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/NavigationModifier.h"
#include "AI/Navigation/NavLinkDefinition.h"
#include "AI/Navigation/NavigationTypes.h"

class UBodySetup;
class UNavCollision;
class AActor;
struct FCompositeNavModifier;

#if WITH_PHYSX
namespace physx
{
	class PxTriangleMesh;
	class PxConvexMesh;
	class PxHeightField;
}
#endif // WITH_PHYSX

struct FNavigableGeometryExport
{
	virtual ~FNavigableGeometryExport() {}
#if WITH_PHYSX
	virtual void ExportPxTriMesh16Bit(physx::PxTriangleMesh const * const TriMesh, const FTransform& LocalToWorld) = 0;
	virtual void ExportPxTriMesh32Bit(physx::PxTriangleMesh const * const TriMesh, const FTransform& LocalToWorld) = 0;
	virtual void ExportPxConvexMesh(physx::PxConvexMesh const * const ConvexMesh, const FTransform& LocalToWorld) = 0;
	virtual void ExportPxHeightField(physx::PxHeightField const * const HeightField, const FTransform& LocalToWorld) = 0;
#endif // WITH_PHYSX
	virtual void ExportHeightFieldSlice(const FNavHeightfieldSamples& PrefetchedHeightfieldSamples, const int32 NumRows, const int32 NumCols, const FTransform& LocalToWorld, const FBox& SliceBox) = 0;
	virtual void ExportRigidBodySetup(UBodySetup& BodySetup, const FTransform& LocalToWorld) = 0;
	virtual void ExportCustomMesh(const FVector* VertexBuffer, int32 NumVerts, const int32* IndexBuffer, int32 NumIndices, const FTransform& LocalToWorld) = 0;
	
	virtual void AddNavModifiers(const FCompositeNavModifier& Modifiers) = 0;

	// Optional delegate for geometry per instance transforms
	virtual void SetNavDataPerInstanceTransformDelegate(const FNavDataPerInstanceTransformDelegate& InDelegate) = 0;
};


namespace NavigationHelper
{
	void GatherCollision(UBodySetup* RigidBody, TNavStatArray<FVector>& OutVertexBuffer, TNavStatArray<int32>& OutIndexBuffer, const FTransform& ComponentToWorld = FTransform::Identity);
	void GatherCollision(UBodySetup* RigidBody, UNavCollision* NavCollision);

	DECLARE_DELEGATE_ThreeParams(FNavLinkProcessorDelegate, FCompositeNavModifier*, const AActor*, const TArray<FNavigationLink>&);
	DECLARE_DELEGATE_ThreeParams(FNavLinkSegmentProcessorDelegate, FCompositeNavModifier*, const AActor*, const TArray<FNavigationSegmentLink>&);

	/** Set new implementation of nav link processor, a function that will be
	 *	be used to process/transform links before adding them to CompositeModifier.
	 *	This function is supposed to be called once during the engine/game 
	 *	setup phase. Not intended to be toggled at runtime */
	ENGINE_API void SetNavLinkProcessorDelegate(const FNavLinkProcessorDelegate& NewDelegate);
	ENGINE_API void SetNavLinkSegmentProcessorDelegate(const FNavLinkSegmentProcessorDelegate& NewDelegate);

	/** called to do any necessary processing on NavLinks and put results in CompositeModifier */
	ENGINE_API void ProcessNavLinkAndAppend(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationLink>& IN NavLinks);

	/** called to do any necessary processing on NavLinks and put results in CompositeModifier */
	ENGINE_API void ProcessNavLinkSegmentAndAppend(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationSegmentLink>& IN NavLinks);

	ENGINE_API void DefaultNavLinkProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationLink>& IN NavLinks);

	ENGINE_API void DefaultNavLinkSegmentProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationSegmentLink>& IN NavLinks);
}
