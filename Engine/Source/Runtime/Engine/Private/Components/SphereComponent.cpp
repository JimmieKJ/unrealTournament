// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "Components/SphereComponent.h"

USphereComponent::USphereComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SphereRadius = 32.0f;
	ShapeColor = FColor(255, 0, 0, 255);

	bUseEditorCompositing = true;
}

FBoxSphereBounds USphereComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	return FBoxSphereBounds( FVector::ZeroVector, FVector(SphereRadius), SphereRadius ).TransformBy(LocalToWorld);
}

void USphereComponent::CalcBoundingCylinder(float& CylinderRadius, float& CylinderHalfHeight) const
{
	CylinderRadius = SphereRadius * ComponentToWorld.GetMaximumAxisScale();
	CylinderHalfHeight = CylinderRadius;
}

void USphereComponent::UpdateBodySetup()
{
	if (ShapeBodySetup == NULL || ShapeBodySetup->IsPendingKill())
	{
		ShapeBodySetup = NewObject<UBodySetup>(this);
		ShapeBodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		ShapeBodySetup->AggGeom.SphereElems.Add(FKSphereElem());
	}

	check (ShapeBodySetup->AggGeom.SphereElems.Num() == 1);
	FKSphereElem* se = ShapeBodySetup->AggGeom.SphereElems.GetData();

	// check for mal formed values
	float Radius = SphereRadius;
	if( Radius < KINDA_SMALL_NUMBER )
	{
		Radius = 0.1f;
	}

	// now set the PhysX data values
	se->Center = FVector::ZeroVector;
	se->Radius = Radius;
}

void USphereComponent::SetSphereRadius( float InSphereRadius, bool bUpdateOverlaps )
{
	SphereRadius = InSphereRadius;
	MarkRenderStateDirty();

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

bool USphereComponent::IsZeroExtent() const
{
	return SphereRadius == 0.f;
}


FPrimitiveSceneProxy* USphereComponent::CreateSceneProxy()
{
	/** Represents a DrawLightRadiusComponent to the scene manager. */
	class FSphereSceneProxy : public FPrimitiveSceneProxy
	{
	public:

		/** Initialization constructor. */
		FSphereSceneProxy(const USphereComponent* InComponent)
			:	FPrimitiveSceneProxy(InComponent)
			,	bDrawOnlyIfSelected( InComponent->bDrawOnlyIfSelected )
			,	SphereColor(InComponent->ShapeColor)
			,	ShapeMaterial(InComponent->ShapeMaterial)
			,	SphereRadius(InComponent->SphereRadius)
		{
			bWillEverBeLit = false;
		}

		  // FPrimitiveSceneProxy interface.
		
		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			QUICK_SCOPE_CYCLE_COUNTER( STAT_SphereSceneProxy_GetDynamicMeshElements );

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];
					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

					const FMatrix& LocalToWorld = GetLocalToWorld();
					int32 SphereSides =  FMath::Clamp<int32>(SphereRadius/4.f, 16, 64);
					if(ShapeMaterial && !View->Family->EngineShowFlags.Wireframe)
					{
						GetSphereMesh(LocalToWorld.GetOrigin(), FVector(SphereRadius), SphereSides, SphereSides/2, ShapeMaterial->GetRenderProxy(false), SDPG_World, false, ViewIndex, Collector);
					}
					else
					{
						const FLinearColor DrawSphereColor = GetViewSelectionColor(SphereColor, *View, IsSelected(), IsHovered(), false, IsIndividuallySelected() );

						float AbsScaleX = LocalToWorld.GetScaledAxis(EAxis::X).Size();
						float AbsScaleY = LocalToWorld.GetScaledAxis(EAxis::Y).Size();
						float AbsScaleZ = LocalToWorld.GetScaledAxis(EAxis::Z).Size();
						float MinAbsScale = FMath::Min3(AbsScaleX, AbsScaleY, AbsScaleZ);

						FVector ScaledX = LocalToWorld.GetUnitAxis(EAxis::X) * MinAbsScale;
						FVector ScaledY = LocalToWorld.GetUnitAxis(EAxis::Y) * MinAbsScale;
						FVector ScaledZ = LocalToWorld.GetUnitAxis(EAxis::Z) * MinAbsScale;

						DrawCircle(PDI, LocalToWorld.GetOrigin(), ScaledX, ScaledY, DrawSphereColor, SphereRadius, SphereSides, SDPG_World);
						DrawCircle(PDI, LocalToWorld.GetOrigin(), ScaledX, ScaledZ, DrawSphereColor, SphereRadius, SphereSides, SDPG_World);
						DrawCircle(PDI, LocalToWorld.GetOrigin(), ScaledY, ScaledZ, DrawSphereColor, SphereRadius, SphereSides, SDPG_World);
					}
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)  override
		{
			const bool bVisibleForSelection = !bDrawOnlyIfSelected || IsSelected();
			const bool bVisibleForShowFlags = true; // @TODO

			FPrimitiveViewRelevance Result;
			Result.bDrawRelevance = IsShown(View) && bVisibleForSelection && bVisibleForShowFlags;
			Result.bDynamicRelevance = true;
			Result.bShadowRelevance = IsShadowCast(View);
			Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			return Result;
		}

		virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
		uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	private:
		const uint32				bDrawOnlyIfSelected:1;
		const FColor				SphereColor;
		const UMaterialInterface*	ShapeMaterial;
		const float					SphereRadius;
	};

	return new FSphereSceneProxy( this );
}


FCollisionShape USphereComponent::GetCollisionShape(float Inflation) const
{
	const float Radius = FMath::Max(0.f, GetScaledSphereRadius() + Inflation);
	return FCollisionShape::MakeSphere(Radius);
}

bool USphereComponent::AreSymmetricRotations(const FQuat& A, const FQuat& B, const FVector& Scale3D) const
{
	// All rotations are equal when scale is uniform.
	// Not detecting rotations around non-uniform scale.
	return Scale3D.GetAbs().AllComponentsEqual() || A.Equals(B);
}
