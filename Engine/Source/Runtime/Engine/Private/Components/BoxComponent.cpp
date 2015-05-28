// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "Components/BoxComponent.h"

UBoxComponent::UBoxComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BoxExtent = FVector(32.0f, 32.0f, 32.0f);

	bUseEditorCompositing = true;
}


void UBoxComponent::SetBoxExtent(FVector NewBoxExtent, bool bUpdateOverlaps)
{
	BoxExtent = NewBoxExtent;
	MarkRenderStateDirty();

	// do this if already created
	// otherwise, it hasn't been really created yet
	if (bPhysicsStateCreated)
	{
		DestroyPhysicsState();
		UpdateBodySetup();
		CreatePhysicsState();

		if ( bUpdateOverlaps && IsCollisionEnabled() && GetOwner() )
		{
			UpdateOverlaps();
		}
	}
}

void UBoxComponent::UpdateBodySetup()
{
	if(ShapeBodySetup == NULL || ShapeBodySetup->IsPendingKill())
	{
		ShapeBodySetup = NewObject<UBodySetup>(this);
		ShapeBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		ShapeBodySetup->AggGeom.BoxElems.Add(FKBoxElem());
	}

	check(ShapeBodySetup->AggGeom.BoxElems.Num() == 1);
	FKBoxElem* se = ShapeBodySetup->AggGeom.BoxElems.GetData();

	// @todo UE4 do we allow this now?
	// check for malformed values
	if( BoxExtent.X < KINDA_SMALL_NUMBER )
	{
		BoxExtent.X = 1.0f;
	}

	if( BoxExtent.Y < KINDA_SMALL_NUMBER )
	{
		BoxExtent.Y = 1.0f;
	}

	if( BoxExtent.Z < KINDA_SMALL_NUMBER )
	{
		BoxExtent.Z = 1.0f;
	}

	// now set the PhysX data values
	se->SetTransform( FTransform::Identity );
	se->X = BoxExtent.X*2;
	se->Y = BoxExtent.Y*2;
	se->Z = BoxExtent.Z*2;
}

bool UBoxComponent::IsZeroExtent() const
{
	return BoxExtent.IsZero();
}

FBoxSphereBounds UBoxComponent::CalcBounds(const FTransform& LocalToWorld) const 
{
	return FBoxSphereBounds( FBox( -BoxExtent, BoxExtent ) ).TransformBy(LocalToWorld);
}



FPrimitiveSceneProxy* UBoxComponent::CreateSceneProxy()
{
	/** Represents a UCapsuleComponent to the scene manager. */
	class FBoxSceneProxy : public FPrimitiveSceneProxy
	{
	public:
		FBoxSceneProxy(const UBoxComponent* InComponent)
			:	FPrimitiveSceneProxy(InComponent)
			,	bDrawOnlyIfSelected( InComponent->bDrawOnlyIfSelected )
			,   BoxExtents( InComponent->BoxExtent )
			,	BoxColor( InComponent->ShapeColor )
		{
			bWillEverBeLit = false;
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			QUICK_SCOPE_CYCLE_COUNTER( STAT_BoxSceneProxy_GetDynamicMeshElements );

			const FMatrix& LocalToWorld = GetLocalToWorld();
			
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];

					const FLinearColor DrawColor = GetViewSelectionColor(BoxColor, *View, IsSelected(), IsHovered(), false, IsIndividuallySelected() );

					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
					DrawOrientedWireBox(PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetScaledAxis( EAxis::X ), LocalToWorld.GetScaledAxis( EAxis::Y ), LocalToWorld.GetScaledAxis( EAxis::Z ), BoxExtents, DrawColor, SDPG_World);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
		{
			const bool bVisible = !bDrawOnlyIfSelected || IsSelected();

			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View) && bVisible;
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = IsShadowCast(View);
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}
		virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
		uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	private:
		const uint32	bDrawOnlyIfSelected:1;
		const FVector	BoxExtents;
		const FColor	BoxColor;
	};

	return new FBoxSceneProxy( this );
}


FCollisionShape UBoxComponent::GetCollisionShape(float Inflation) const
{
	FVector Extent = GetScaledBoxExtent() + Inflation;
	if (Inflation < 0.f)
	{
		// Don't shrink below zero size.
		Extent = Extent.ComponentMax(FVector::ZeroVector);
	}

	return FCollisionShape::MakeBox(Extent);
}
