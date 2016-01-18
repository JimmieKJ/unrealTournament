// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "NavDataGenerator.h"
#include "NavigationOctree.h"
#if WITH_RECAST
#include "RecastNavMeshGenerator.h"
#endif // WITH_RECAST
#include "AI/NavigationSystemHelpers.h"
#include "VisualLogger/VisualLogger.h"
#include "AI/Navigation/NavCollision.h"

namespace NavigationHelper
{
	void GatherCollision(UBodySetup* RigidBody, TNavStatArray<FVector>& OutVertexBuffer, TNavStatArray<int32>& OutIndexBuffer, const FTransform& LocalToWorld)
	{
		if (RigidBody == NULL)
		{
			return;
		}
#if WITH_RECAST
		FRecastNavMeshGenerator::ExportRigidBodyGeometry(*RigidBody, OutVertexBuffer, OutIndexBuffer, LocalToWorld);
#endif // WITH_RECAST
	}

	void GatherCollision(UBodySetup* RigidBody, UNavCollision* NavCollision)
	{
		if (RigidBody == NULL || NavCollision == NULL)
		{
			return;
		}
#if WITH_RECAST
		FRecastNavMeshGenerator::ExportRigidBodyGeometry(*RigidBody
			, NavCollision->TriMeshCollision.VertexBuffer, NavCollision->TriMeshCollision.IndexBuffer
			, NavCollision->ConvexCollision.VertexBuffer, NavCollision->ConvexCollision.IndexBuffer
			, NavCollision->ConvexShapeIndices);
#endif // WITH_RECAST
	}

	FORCEINLINE_DEBUGGABLE float RawGeometryFall(const AActor* Querier, const FVector& FallStart, const float FallLimit)
	{
		float FallDownHeight = 0.f;

		UE_VLOG_SEGMENT(Querier, LogNavigation, Log, FallStart, FallStart + FVector(0, 0, -FallLimit)
				, FColor::Red, TEXT("TerrainTrace"));

		FCollisionQueryParams TraceParams(NAME_None, true, Querier);
		FHitResult Hit;
		const bool bHit = Querier->GetWorld()->LineTraceSingleByObjectType(Hit, FallStart, FallStart+FVector(0,0,-FallLimit), FCollisionObjectQueryParams(ECC_WorldStatic), TraceParams);
		if( bHit )
		{
			UE_VLOG_LOCATION(Querier, LogNavigation, Log, Hit.Location, 15, FColor::Red, TEXT("%s")
				, Hit.Actor.IsValid() ? *Hit.Actor->GetName() : TEXT("NULL"));

			if (Cast<UStaticMeshComponent>(Hit.Component.Get()))
			{
				const FVector Loc = Hit.ImpactPoint;
				FallDownHeight = FallStart.Z - Loc.Z;
			}
		}

		return FallDownHeight;
	}

	void DefaultNavLinkProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationLink>& IN NavLinks)
	{
		const FTransform LocalToWorld = Actor->ActorToWorld();
		FSimpleLinkNavModifier SimpleLink(NavLinks, LocalToWorld);

		// adjust links
		for (int32 LinkIndex = 0; LinkIndex < SimpleLink.Links.Num(); ++LinkIndex)
		{
			FNavigationLink& Link = SimpleLink.Links[LinkIndex];

			// this one needs adjusting
			if (Link.Direction == ENavLinkDirection::RightToLeft)
			{
				Swap(Link.Left, Link.Right);
			}

			if (Link.MaxFallDownLength > 0)
			{
				const FVector WorldRight = LocalToWorld.TransformPosition(Link.Right);
				const float FallDownHeight = RawGeometryFall(Actor, WorldRight, Link.MaxFallDownLength);

				if (FallDownHeight > 0.f)
				{
					// @todo maybe it's a good idea to clear ModifiedLink.MaxFallDownLength here
					UE_VLOG_SEGMENT(Actor, LogNavigation, Log, WorldRight, WorldRight + FVector(0, 0, -FallDownHeight)
						, FColor::Green, TEXT("FallDownHeight %d"), LinkIndex);

					Link.Right.Z -= FallDownHeight;
				}
			}

			if (Link.LeftProjectHeight > 0)
			{
				const FVector WorldLeft = LocalToWorld.TransformPosition(Link.Left);
				const float FallDownHeight = RawGeometryFall(Actor, WorldLeft, Link.LeftProjectHeight);

				if (FallDownHeight > 0.f)
				{
					// @todo maybe it's a good idea to clear ModifiedLink.LeftProjectHeight here
					UE_VLOG_SEGMENT(Actor, LogNavigation, Log, WorldLeft, WorldLeft + FVector(0, 0, -FallDownHeight)
						, FColor::Green, TEXT("LeftProjectHeight %d"), LinkIndex);

					Link.Left.Z -= FallDownHeight;
				}
			}
		}

		CompositeModifier->Add(SimpleLink);
	}

	void DefaultNavLinkSegmentProcessorImpl(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationSegmentLink>& IN NavLinks)
	{
		const FTransform LocalToWorld = Actor->ActorToWorld();
		FSimpleLinkNavModifier SimpleLink(NavLinks, LocalToWorld);

			// adjust links if needed
		for (int32 LinkIndex = 0; LinkIndex < SimpleLink.SegmentLinks.Num(); ++LinkIndex)
		{
			FNavigationSegmentLink& Link = SimpleLink.SegmentLinks[LinkIndex];

			// this one needs adjusting
			if (Link.Direction == ENavLinkDirection::RightToLeft)
			{
				Swap(Link.LeftStart, Link.RightStart);
				Swap(Link.LeftEnd, Link.RightEnd);
			}

			if (Link.MaxFallDownLength > 0)
			{
				const FVector WorldRightStart = LocalToWorld.TransformPosition(Link.RightStart);
				const FVector WorldRightEnd = LocalToWorld.TransformPosition(Link.RightEnd);

				const float FallDownHeightStart = RawGeometryFall(Actor, WorldRightStart, Link.MaxFallDownLength);
				const float FallDownHeightEnd = RawGeometryFall(Actor, WorldRightEnd, Link.MaxFallDownLength);

				if (FallDownHeightStart > 0.f)
				{
					// @todo maybe it's a good idea to clear ModifiedLink.MaxFallDownLength here
					UE_VLOG_SEGMENT(Actor, LogNavigation, Log, WorldRightStart, WorldRightStart + FVector(0, 0, -FallDownHeightStart)
						, FColor::Green, TEXT("FallDownHeightStart %d"), LinkIndex);

					Link.RightStart.Z -= FallDownHeightStart;
				}
				if (FallDownHeightEnd > 0.f)
				{
					// @todo maybe it's a good idea to clear ModifiedLink.MaxFallDownLength here
					UE_VLOG_SEGMENT(Actor, LogNavigation, Log, WorldRightEnd, WorldRightEnd + FVector(0, 0, -FallDownHeightEnd)
						, FColor::Green, TEXT("FallDownHeightEnd %d"), LinkIndex);

					Link.RightEnd.Z -= FallDownHeightEnd;
				}
			}
		}

		CompositeModifier->Add(SimpleLink);
	}

	FNavLinkProcessorDelegate NavLinkProcessor = FNavLinkProcessorDelegate::CreateStatic(DefaultNavLinkProcessorImpl);
	FNavLinkSegmentProcessorDelegate NavLinkSegmentProcessor = FNavLinkSegmentProcessorDelegate::CreateStatic(DefaultNavLinkSegmentProcessorImpl);

	void ProcessNavLinkAndAppend(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationLink>& IN NavLinks)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_AdjustingNavLinks);

		if (NavLinks.Num())
		{
			check(NavLinkProcessor.IsBound());
			NavLinkProcessor.Execute(CompositeModifier, Actor, NavLinks);
		}
	}

	void ProcessNavLinkSegmentAndAppend(FCompositeNavModifier* OUT CompositeModifier, const AActor* Actor, const TArray<FNavigationSegmentLink>& IN NavLinks)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_AdjustingNavLinks);

		if (NavLinks.Num())
		{
			check(NavLinkSegmentProcessor.IsBound());
			NavLinkSegmentProcessor.Execute(CompositeModifier, Actor, NavLinks);
		}
	}

	void SetNavLinkProcessorDelegate(const FNavLinkProcessorDelegate& NewDelegate)
	{
		check(NewDelegate.IsBound());
		NavLinkProcessor = NewDelegate;
	}

	void SetNavLinkSegmentProcessorDelegate(const FNavLinkSegmentProcessorDelegate& NewDelegate)
	{
		check(NewDelegate.IsBound());
		NavLinkSegmentProcessor = NewDelegate;
	}
}
