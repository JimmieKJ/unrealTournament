// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTTrophyRoom.h"
#include "UTTrophyRoomRenderingComponent.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API UTTrophyRoomRenderingProxy : public FPrimitiveSceneProxy
{
private:

	float CapsuleRadius;
	float CapsuleHalfHeight;
	float ArrowHeadHeight;
	float ArrowHeadWidth;

	TArray<FTrophyGroup> TeamGroups;

public:

	UTTrophyRoomRenderingProxy(const UPrimitiveComponent* InComponent) : FPrimitiveSceneProxy(InComponent)
	{
		CapsuleRadius = 42.f;
		CapsuleHalfHeight = 92.0f;
		ArrowHeadHeight = 60.0f;
		ArrowHeadWidth = 20.0f;

		AUTTrophyRoom* TrophyRoom = Cast<AUTTrophyRoom>(InComponent->GetOwner());
		if (TrophyRoom != nullptr)
		{
			TeamGroups = TrophyRoom->TeamGroups;
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		const FMatrix& LocalToWorld = GetLocalToWorld();
		const int32 CapsuleSides = FMath::Clamp<int32>(CapsuleRadius / 4.f, 16, 64);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
				
				for (auto& Group : TeamGroups)
				{
					FLinearColor DrawColor = FLinearColor::White;
					if (Group.Team == 0)
					{
						DrawColor = FLinearColor::Red;
					}
					else if (Group.Team == 1)
					{
						DrawColor = FLinearColor::Blue;
					}

					for (auto& PlayerTransform : Group.Players)
					{
						FMatrix PlrMat = PlayerTransform.ToMatrixNoScale();

						DrawWireCapsule(PDI, LocalToWorld.TransformPosition(PlayerTransform.GetLocation()), LocalToWorld.GetScaledAxis(EAxis::X), LocalToWorld.GetScaledAxis(EAxis::Y), LocalToWorld.GetScaledAxis(EAxis::Z), DrawColor, CapsuleRadius, CapsuleHalfHeight, CapsuleSides, SDPG_World);
						DrawDirectionalArrow(PDI, PlayerTransform.ToMatrixNoScale() * LocalToWorld, DrawColor, ArrowHeadHeight, ArrowHeadWidth, SDPG_Foreground);
					}
				}
			}
		}
	}

	virtual uint32 GetMemoryFootprint(void) const
	{
		return(sizeof(*this));
	}

	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bEditorPrimitiveRelevance = false;// UseEditorCompositing(View);
		return Result;
	}
};

#endif


UUTTrophyRoomRenderingComponent::UUTTrophyRoomRenderingComponent(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Allows updating in game, while optimizing rendering for the case that it is not modified
	Mobility = EComponentMobility::Stationary;

	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	AlwaysLoadOnClient = false;
	AlwaysLoadOnServer = false;
	bHiddenInGame = true;
	bGenerateOverlapEvents = false;
}

FBoxSphereBounds UUTTrophyRoomRenderingComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBox Bounds(0);

	if (GExitPurge || HasAnyFlags(RF_BeginDestroyed))
	{
		return FBoxSphereBounds(Bounds);
	}

	AUTTrophyRoom* TrophyRoom = Cast<AUTTrophyRoom>(GetOwner());
	if (TrophyRoom != nullptr)
	{
		Bounds += TrophyRoom->GetActorLocation();

		for (auto& Group : TrophyRoom->TeamGroups)
		{
//			Bounds += LocalToWorld.TransformPosition(Group.Camera.GetLocation());
			for (auto& Player : Group.Players)
			{
				Bounds += LocalToWorld.TransformPosition(Player.GetLocation());
			}
		}
	}
	return FBoxSphereBounds(Bounds);
}

FPrimitiveSceneProxy* UUTTrophyRoomRenderingComponent::CreateSceneProxy()
{
#if UE_SERVER
	return nullptr;
#else
	return new UTTrophyRoomRenderingProxy(this);
#endif
}


