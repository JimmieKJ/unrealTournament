// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "FunctionalTestingPrivatePCH.h"
//#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"

//----------------------------------------------------------------------//
// FFTestRenderingSceneProxy
//----------------------------------------------------------------------//
class FFTestRenderingSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** Initialization constructor. */
	FFTestRenderingSceneProxy(const UFuncTestRenderingComponent& InComponent)
		: FPrimitiveSceneProxy(&InComponent)
	{
		OwningTest = Cast<AFunctionalTest>(InComponent.GetOwner());
		if (OwningTest)
		{
			TArray<AActor*> RelevantActors;
			OwningTest->GatherRelevantActors(RelevantActors);
			
			TestActorLocation = OwningTest->GetActorLocation();
			for (auto Actor : RelevantActors)
			{
				if (Actor)
				{
					Locations.Add(Actor->GetActorLocation());
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && IsSelected();
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucencyRelevance = IsShown(View);
		return Result;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		static const FLinearColor RadiusColor = FLinearColor(FColorList::Orange);
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				for (auto Location : Locations)
				{
					PDI->DrawLine(TestActorLocation, Location, RadiusColor, SDPG_World, 3.f);
					DrawArrowHead(PDI, Location, TestActorLocation, 30.f, RadiusColor, SDPG_World, 3.f);
				}
			}
		}
	}

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return FPrimitiveSceneProxy::GetAllocatedSize() + Locations.GetAllocatedSize(); }

protected:
	AFunctionalTest* OwningTest;
	FVector TestActorLocation;
	TArray<FVector> Locations;
};
//----------------------------------------------------------------------//
// UFuncTestRenderingComponent
//----------------------------------------------------------------------//
UFuncTestRenderingComponent::UFuncTestRenderingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Allows updating in game, while optimizing rendering for the case that it is not modified
	Mobility = EComponentMobility::Stationary;

	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	AlwaysLoadOnClient = false;
	AlwaysLoadOnServer = false;

	bGenerateOverlapEvents = false;
}

FPrimitiveSceneProxy* UFuncTestRenderingComponent::CreateSceneProxy()
{
	return new FFTestRenderingSceneProxy(*this);
}

FBoxSphereBounds UFuncTestRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const AFunctionalTest* Owner = Cast<AFunctionalTest>(GetOwner());
	if (Owner)
	{
		FBox BoundingBox;

		TArray<AActor*> RelevantActors;
		Owner->GatherRelevantActors(RelevantActors);

		BoundingBox += Owner->GetActorLocation();
		for (auto Actor : RelevantActors)
		{
			if (Actor)
			{
				BoundingBox += Actor->GetActorLocation();
			}
		}

		return FBoxSphereBounds(BoundingBox);
	}

	return FBoxSphereBounds(EForceInit::ForceInitToZero);
}
