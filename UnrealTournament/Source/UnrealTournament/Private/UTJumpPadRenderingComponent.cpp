// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTJumpPad.h"
#include "UTJumpPadRenderingComponent.h"

class UNREALTOURNAMENT_API UTJumpPadRenderingProxy : public FPrimitiveSceneProxy
{
private:
	FVector JumpPadLocation;
	FVector JumpPadTarget;
	FVector JumpVelocity;
	float	JumpTime;
	float	GravityZ;

public:

	UTJumpPadRenderingProxy(const UPrimitiveComponent* InComponent) : FPrimitiveSceneProxy(InComponent)
	{
		AUTJumpPad *JumpPad = Cast<AUTJumpPad>(InComponent->GetOwner());

		if (JumpPad != NULL)
		{
			JumpPadLocation = InComponent->GetOwner()->GetActorLocation();
			JumpVelocity = JumpPad->CalculateJumpVelocity(JumpPad);
			JumpTime = JumpPad->JumpTime;
			GravityZ = JumpPad->GetWorld()->GetGravityZ();
			JumpPadTarget = JumpPad->JumpTarget;
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				static const float LINE_THICKNESS = 20;
				static const int32 NUM_DRAW_LINES = 16;
				static const float FALL_DAMAGE_VELOCITY = GetDefault<AUTCharacter>()->MaxSafeFallSpeed; // TODO: find default pawn class and pull from that

				FVector Start = JumpPadLocation;
				float TimeTick = JumpTime / NUM_DRAW_LINES;

				for (int32 i = 1; i <= NUM_DRAW_LINES; i++)
				{
					//Find the position in the Trajectory
					float TimeElapsed = TimeTick * i;
					FVector End = JumpPadLocation + (JumpVelocity * TimeElapsed);
					End.Z -= (-GravityZ * FMath::Pow(TimeElapsed, 2)) / 2;

					//Colour gradient to show how fast the player will be travelling. Useful to visualize fall damage
					float speed = FMath::Clamp(FMath::Abs(Start.Z - End.Z) / TimeTick / FALL_DAMAGE_VELOCITY, 0.0f, 1.0f);
					FColor LineClr = FColor::MakeRedToGreenColorFromScalar(1 - speed);

					//Draw and swap line ends
					PDI->DrawLine(Start, End, LineClr, 0, LINE_THICKNESS);
					Start = End;
				}
			}
		}
	}

	virtual uint32 GetMemoryFootprint(void) const
	{
		return(sizeof(*this));
	}

	FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && (IsSelected() || View->Family->EngineShowFlags.Navigation);
		Result.bDynamicRelevance = true;
		Result.bNormalTranslucencyRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}
};




UUTJumpPadRenderingComponent::UUTJumpPadRenderingComponent(const class FObjectInitializer& ObjectInitializer)
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

FBoxSphereBounds UUTJumpPadRenderingComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	FBox Bounds(0);

	if (GExitPurge || HasAnyFlags(RF_BeginDestroyed))
	{
		return FBoxSphereBounds(Bounds);
	}

	AUTJumpPad *JumpPad = Cast<AUTJumpPad>(GetOwner());

	if (JumpPad != NULL)
	{
		FVector JumpPadLocation = JumpPad->GetActorLocation();
		FVector JumpPadTarget = JumpPad->JumpTarget;
		FVector JumpVelocity = JumpPad->CalculateJumpVelocity(JumpPad);
		float JumpTime = JumpPad->JumpTime;
		float GravityZ = -JumpPad->GetWorld()->GetGravityZ();

		Bounds += JumpPadLocation;
		Bounds += JumpPad->ActorToWorld().TransformPosition(JumpPadTarget);
		
		//Guard divide by zero potential with gravity
		if (JumpPad->GetWorld()->GetGravityZ() != 0.0f)
		{
			//If the apex of the jump is within the Pad and destination add to the bounds
			float ApexTime = JumpVelocity.Z / GravityZ;
			if (ApexTime > 0.0f && ApexTime < JumpTime)
			{
				FVector Apex = JumpPadLocation + (JumpVelocity * ApexTime);
				Apex.Z -= (GravityZ * FMath::Pow(ApexTime, 2)) / 2;
				Bounds += Apex;
			}
		}
	}
	return FBoxSphereBounds(Bounds);
}

FPrimitiveSceneProxy* UUTJumpPadRenderingComponent::CreateSceneProxy()
{
	return new UTJumpPadRenderingProxy(this);
}


