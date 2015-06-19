// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Collision.cpp: AActor collision implementation
=============================================================================*/

#include "EnginePrivate.h"
#include "Collision.h"

//////////////////////////////////////////////////////////////////////////
// FHitResult

FHitResult::FHitResult(class AActor* InActor, class UPrimitiveComponent* InComponent, FVector const& HitLoc, FVector const& HitNorm)
{
	FMemory::Memzero(this, sizeof(FHitResult));
	Location = HitLoc;
	ImpactPoint = HitLoc;
	Normal = HitNorm;
	ImpactNormal = HitNorm;
	Actor = InActor;
	Component = InComponent;
}

AActor* FHitResult::GetActor() const
{
	return Actor.Get();
}

UPrimitiveComponent* FHitResult::GetComponent() const
{
	return Component.Get();
}

bool FHitResult::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// Most of the time the vectors are the same values, use that as an optimization
	bool bImpactPointEqualsLocation = 0, bImpactNormalEqualsNormal = 0;

	// Often times the indexes are invalid, use that as an optimization
	bool bInvalidItem = 0, bInvalidFaceIndex = 0, bNoPenetrationDepth = 0;

	if (Ar.IsSaving())
	{
		bImpactPointEqualsLocation = (ImpactPoint == Location);
		bImpactNormalEqualsNormal = (ImpactNormal == Normal);
		bInvalidItem = (Item == INDEX_NONE);
		bInvalidFaceIndex = (FaceIndex == INDEX_NONE);
		bNoPenetrationDepth = (PenetrationDepth == 0.0f);
	}

	// pack bitfield with flags
	uint8 Flags = (bBlockingHit << 0) | (bStartPenetrating << 1) | (bImpactPointEqualsLocation << 2) | (bImpactNormalEqualsNormal << 3) | (bInvalidItem << 4) | (bInvalidFaceIndex << 5) | (bInvalidFaceIndex << 6);
	Ar.SerializeBits(&Flags, 7); 
	bBlockingHit = (Flags & (1 << 0)) ? 1 : 0;
	bStartPenetrating = (Flags & (1 << 1)) ? 1 : 0;
	bImpactPointEqualsLocation = (Flags & (1 << 2)) ? 1 : 0;
	bImpactNormalEqualsNormal = (Flags & (1 << 3)) ? 1 : 0;
	bInvalidItem = (Flags & (1 << 4)) ? 1 : 0;
	bInvalidFaceIndex = (Flags & (1 << 5)) ? 1 : 0;
	bNoPenetrationDepth = (Flags & (1 << 6)) ? 1 : 0;

	Ar << Time;

	bOutSuccess = true;

	bool bOutSuccessLocal = true;

	Location.NetSerialize(Ar, Map, bOutSuccessLocal);
	bOutSuccess &= bOutSuccessLocal;
	Normal.NetSerialize(Ar, Map, bOutSuccessLocal);
	bOutSuccess &= bOutSuccessLocal;

	if (!bImpactPointEqualsLocation)
	{
		ImpactPoint.NetSerialize(Ar, Map, bOutSuccessLocal);
		bOutSuccess &= bOutSuccessLocal;
	}
	else if (Ar.IsLoading())
	{
		ImpactPoint = Location;
	}
	
	if (!bImpactNormalEqualsNormal)
	{
		ImpactNormal.NetSerialize(Ar, Map, bOutSuccessLocal);
		bOutSuccess &= bOutSuccessLocal;
	}
	else if (Ar.IsLoading())
	{
		ImpactNormal = Normal;
	}
	TraceStart.NetSerialize(Ar, Map, bOutSuccessLocal);
	bOutSuccess &= bOutSuccessLocal;
	TraceEnd.NetSerialize(Ar, Map, bOutSuccessLocal);
	bOutSuccess &= bOutSuccessLocal;

	if (!bNoPenetrationDepth)
	{
		Ar << PenetrationDepth;
	}
	else if(Ar.IsLoading())
	{
		PenetrationDepth = 0.0f;
	}
	
	if (!bInvalidItem)
	{
		Ar << Item;
	}
	else if (Ar.IsLoading())
	{
		Item = INDEX_NONE;
	}

	Ar << PhysMaterial;
	Ar << Actor;
	Ar << Component;
	Ar << BoneName;
	if (!bInvalidFaceIndex)
	{
		Ar << FaceIndex;
	}
	else if (Ar.IsLoading())
	{
		FaceIndex = INDEX_NONE;
	}
	

	return true;
}

//////////////////////////////////////////////////////////////////////////
// FOverlapResult

AActor* FOverlapResult::GetActor() const
{
	return Actor.Get();
}

UPrimitiveComponent* FOverlapResult::GetComponent() const
{
	return Component.Get();
}


//////////////////////////////////////////////////////////////////////////
// FCollisionQueryParams

FCollisionQueryParams::FCollisionQueryParams(FName InTraceTag, bool bInTraceComplex, const AActor* InIgnoreActor)
{
	bTraceComplex = bInTraceComplex;
	TraceTag = InTraceTag;
	bTraceAsyncScene = false;
	bFindInitialOverlaps = true;
	bReturnFaceIndex = false;
	bReturnPhysicalMaterial = false;

	AddIgnoredActor(InIgnoreActor);
	if (InIgnoreActor != NULL)
	{
		OwnerTag = InIgnoreActor->GetFName();
	}
}

void FCollisionQueryParams::AddIgnoredActor(const AActor* InIgnoreActor)
{
	if (InIgnoreActor)
	{
		if (IgnoreComponents.Num() == 0)
		{
			// Don't need to AddUnique if we start empty, as GetComponents should never contain duplicate components.
			for (const UActorComponent* ActorComponent : InIgnoreActor->GetComponents())
			{
				const UPrimitiveComponent* PrimComponent = Cast<const UPrimitiveComponent>(ActorComponent);
				if (PrimComponent)
				{
					IgnoreComponents.Add(PrimComponent->GetUniqueID());
				}
			}
		}
		else
		{
			for (const UActorComponent* ActorComponent : InIgnoreActor->GetComponents())
			{
				const UPrimitiveComponent* PrimComponent = Cast<const UPrimitiveComponent>(ActorComponent);
				if (PrimComponent)
				{
					IgnoreComponents.AddUnique(PrimComponent->GetUniqueID());
				}
			}
		}
	}
}

void FCollisionQueryParams::AddIgnoredActors(const TArray<AActor*>& InIgnoreActors)
{
	for (int32 Idx = 0; Idx < InIgnoreActors.Num(); ++Idx)
	{
		if (AActor const* const A = InIgnoreActors[Idx])
		{
			AddIgnoredActor(A);
		}
	}
}

void FCollisionQueryParams::AddIgnoredActors(const TArray<TWeakObjectPtr<AActor> >& InIgnoreActors)
{
	for (int32 Idx = 0; Idx < InIgnoreActors.Num(); ++Idx)
	{
		if (AActor const* const A = InIgnoreActors[Idx].Get())
		{
			AddIgnoredActor(A);
		}
	}
}


void FCollisionQueryParams::AddIgnoredComponent(const UPrimitiveComponent* InIgnoreComponent)
{
	if (InIgnoreComponent)
	{
		IgnoreComponents.AddUnique(InIgnoreComponent->GetUniqueID());
	}
}

void FCollisionQueryParams::AddIgnoredComponents(const TArray<UPrimitiveComponent*>& InIgnoreComponents)
{
	for (const UPrimitiveComponent* IgnoreComponent : InIgnoreComponents)
	{
		AddIgnoredComponent(IgnoreComponent);
	}
}

void FCollisionQueryParams::AddIgnoredComponents(const TArray<TWeakObjectPtr<UPrimitiveComponent>>& InIgnoreComponents)
{
	for (TWeakObjectPtr<UPrimitiveComponent> IgnoreComponent : InIgnoreComponents)
	{
		if (IgnoreComponent.IsValid())
		{
			AddIgnoredComponent(IgnoreComponent.Get());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FSeparatingAxisPointCheck

TArray<FVector> FSeparatingAxisPointCheck::TriangleVertices;

bool FSeparatingAxisPointCheck::TestSeparatingAxisCommon(const FVector& Axis, float ProjectedPolyMin, float ProjectedPolyMax)
{
	const float ProjectedCenter = FVector::DotProduct(Axis, BoxCenter);
	const float ProjectedExtent = FVector::DotProduct(Axis.GetAbs(), BoxExtent);
	const float ProjectedBoxMin = ProjectedCenter - ProjectedExtent;
	const float ProjectedBoxMax = ProjectedCenter + ProjectedExtent;

	if (ProjectedPolyMin > ProjectedBoxMax || ProjectedPolyMax < ProjectedBoxMin)
	{
		return false;
	}

	if (bCalcLeastPenetration)
	{
		const float AxisMagnitudeSqr = Axis.SizeSquared();
		if (AxisMagnitudeSqr > (SMALL_NUMBER * SMALL_NUMBER))
		{
			const float InvAxisMagnitude = FMath::InvSqrt(AxisMagnitudeSqr);
			const float MinPenetrationDist = (ProjectedBoxMax - ProjectedPolyMin) * InvAxisMagnitude;
			const float	MaxPenetrationDist = (ProjectedPolyMax - ProjectedBoxMin) * InvAxisMagnitude;

			if (MinPenetrationDist < BestDist)
			{
				BestDist = MinPenetrationDist;
				HitNormal = -Axis * InvAxisMagnitude;
			}

			if (MaxPenetrationDist < BestDist)
			{
				BestDist = MaxPenetrationDist;
				HitNormal = Axis * InvAxisMagnitude;
			}
		}
	}

	return true;
}

bool FSeparatingAxisPointCheck::TestSeparatingAxisTriangle(const FVector& Axis)
{
	const float ProjectedV0 = FVector::DotProduct(Axis, PolyVertices[0]);
	const float ProjectedV1 = FVector::DotProduct(Axis, PolyVertices[1]);
	const float ProjectedV2 = FVector::DotProduct(Axis, PolyVertices[2]);
	const float ProjectedTriMin = FMath::Min3(ProjectedV0, ProjectedV1, ProjectedV2);
	const float ProjectedTriMax = FMath::Max3(ProjectedV0, ProjectedV1, ProjectedV2);

	return TestSeparatingAxisCommon(Axis, ProjectedTriMin, ProjectedTriMax);
}

bool FSeparatingAxisPointCheck::TestSeparatingAxisGeneric(const FVector& Axis)
{
	float ProjectedPolyMin = TNumericLimits<float>::Max();
	float ProjectedPolyMax = TNumericLimits<float>::Lowest();
	for (const auto& Vertex : PolyVertices)
	{
		const float ProjectedVertex = FVector::DotProduct(Axis, Vertex);
		ProjectedPolyMin = FMath::Min(ProjectedPolyMin, ProjectedVertex);
		ProjectedPolyMax = FMath::Max(ProjectedPolyMax, ProjectedVertex);
	}

	return TestSeparatingAxisCommon(Axis, ProjectedPolyMin, ProjectedPolyMax);
}

bool FSeparatingAxisPointCheck::FindSeparatingAxisTriangle()
{
	check(PolyVertices.Num() == 3);
	const FVector EdgeDir0 = PolyVertices[1] - PolyVertices[0];
	const FVector EdgeDir1 = PolyVertices[2] - PolyVertices[1];
	const FVector EdgeDir2 = PolyVertices[0] - PolyVertices[2];

	// Box Z edge x triangle edges.

	if (!TestSeparatingAxisTriangle(FVector(EdgeDir0.Y, -EdgeDir0.X, 0.0f)) ||
		!TestSeparatingAxisTriangle(FVector(EdgeDir1.Y, -EdgeDir1.X, 0.0f)) ||
		!TestSeparatingAxisTriangle(FVector(EdgeDir2.Y, -EdgeDir2.X, 0.0f)))
	{
		return false;
	}

	// Box Y edge x triangle edges.

	if (!TestSeparatingAxisTriangle(FVector(-EdgeDir0.Z, 0.0f, EdgeDir0.X)) ||
		!TestSeparatingAxisTriangle(FVector(-EdgeDir1.Z, 0.0f, EdgeDir1.X)) ||
		!TestSeparatingAxisTriangle(FVector(-EdgeDir2.Z, 0.0f, EdgeDir2.X)))
	{
		return false;
	}

	// Box X edge x triangle edges.

	if (!TestSeparatingAxisTriangle(FVector(0.0f, EdgeDir0.Z, -EdgeDir0.Y)) ||
		!TestSeparatingAxisTriangle(FVector(0.0f, EdgeDir1.Z, -EdgeDir1.Y)) ||
		!TestSeparatingAxisTriangle(FVector(0.0f, EdgeDir2.Z, -EdgeDir2.Y)))
	{
		return false;
	}

	// Box faces.

	if (!TestSeparatingAxisTriangle(FVector(0.0f, 0.0f, 1.0f)) ||
		!TestSeparatingAxisTriangle(FVector(1.0f, 0.0f, 0.0f)) ||
		!TestSeparatingAxisTriangle(FVector(0.0f, 1.0f, 0.0f)))
	{
		return false;
	}

	// Triangle normal.

	if (!TestSeparatingAxisTriangle(FVector::CrossProduct(EdgeDir1, EdgeDir0)))
	{
		return false;
	}

	return true;
}

bool FSeparatingAxisPointCheck::FindSeparatingAxisGeneric()
{
	check(PolyVertices.Num() > 3);
	int32 LastIndex = PolyVertices.Num() - 1;
	for (int32 Index = 0; Index < PolyVertices.Num(); Index++)
	{
		const FVector& V0 = PolyVertices[LastIndex];
		const FVector& V1 = PolyVertices[Index];
		const FVector EdgeDir = V1 - V0;

		// Box edges x polygon edge

		if (!TestSeparatingAxisGeneric(FVector(EdgeDir.Y, -EdgeDir.X, 0.0f)) ||
			!TestSeparatingAxisGeneric(FVector(-EdgeDir.Z, 0.0f, EdgeDir.X)) ||
			!TestSeparatingAxisGeneric(FVector(0.0f, EdgeDir.Z, -EdgeDir.Y)))
		{
			return false;
		}

		LastIndex = Index;
	}

	// Box faces.

	if (!TestSeparatingAxisGeneric(FVector(0.0f, 0.0f, 1.0f)) ||
		!TestSeparatingAxisGeneric(FVector(1.0f, 0.0f, 0.0f)) ||
		!TestSeparatingAxisGeneric(FVector(0.0f, 1.0f, 0.0f)))
	{
		return false;
	}

	// Polygon normal.

	int32 Index0 = PolyVertices.Num() - 2;
	int32 Index1 = Index0 + 1;
	for (int32 Index2 = 0; Index2 < PolyVertices.Num(); Index2++)
	{
		const FVector& V0 = PolyVertices[Index0];
		const FVector& V1 = PolyVertices[Index1];
		const FVector& V2 = PolyVertices[Index2];

		const FVector EdgeDir0 = V1 - V0;
		const FVector EdgeDir1 = V2 - V1;

		FVector Normal = FVector::CrossProduct(EdgeDir1, EdgeDir0);
		if (Normal.SizeSquared() > SMALL_NUMBER)
		{
			if (!TestSeparatingAxisGeneric(Normal))
			{
				return false;
			}
			break;
		}

		Index0 = Index1;
		Index1 = Index2;
	}

	return true;
}

