// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeleporter.h"
#include "UTRecastNavMesh.h"
#include "UTReachSpec_Teleport.h"

AUTTeleporter::AUTTeleporter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	TriggerBox = ObjectInitializer.CreateDefaultSubobject<UBoxComponent>(this, TEXT("TriggerBox"));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = TriggerBox;

#if WITH_EDITORONLY_DATA
	ExitArrow = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UArrowComponent>(this, TEXT("ExitArrow"));
	if (ExitArrow != NULL)
	{
		ExitArrow->SetupAttachment(RootComponent);
		ExitArrow->ArrowColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f).ToFColor(false);
	}

	EntryArrow = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UArrowComponent>(this, TEXT("EntryArrow"));
	if (EntryArrow != NULL)
	{
		EntryArrow->SetupAttachment(RootComponent);
		EntryArrow->ArrowColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f).ToFColor(false);
	}

	bLockTeleportTarget = true;
#endif

	bSetRotation = true;
	TeleportTarget = FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 100.0f));

	// note that we use the Actor event so adding different collision primitives automatically works, as we don't need the more detailed parameters of the component version
	OnActorBeginOverlap.AddDynamic(this, &AUTTeleporter::OnOverlapBegin);

	TeleportTarget.SetIdentity();
}

bool AUTTeleporter::CanTeleport_Implementation(AActor* OtherActor)
{
	AUTCharacter* C = Cast<AUTCharacter>(OtherActor);
	return (C != NULL && !C->IsDead() && !C->IsFeigningDeath() && C->Controller != NULL && !C->UTCharacterMovement->IsProcessingClientAdjustment());
}

// prevents re-entrancy between teleporters
static AActor* CurrentlyTeleportingActor = NULL;

void AUTTeleporter::OnOverlapBegin(AActor* OverlappingActor, AActor* OtherActor)
{
	if (OtherActor != NULL && CurrentlyTeleportingActor != OtherActor && CanTeleport(OtherActor))
	{
		CurrentlyTeleportingActor = OtherActor;

		FTransform MyTransform = FTransform(GetActorRotation());
		FTransform TargetTransform = TeleportTarget * ActorToWorld();
		FVector AdjustedTeleportLoc = TargetTransform.GetLocation();
		FRotator TargetRot = TargetTransform.Rotator();
		FRotator TargetViewRot = TargetRot;
		// don't pitch or roll Pawns
		ACharacter* P = Cast<ACharacter>(OtherActor);
		if (P != NULL)
		{
			TargetRot.Yaw = TargetRot.Yaw + FMath::UnwindDegrees(P->GetActorRotation().Yaw) - FMath::UnwindDegrees(GetActorRotation().Yaw);
			if (P->GetController())
			{
				TargetViewRot = P->GetControlRotation();
				TargetViewRot.Yaw = TargetRot.Yaw;
			}
			TargetRot.Pitch = 0.0f;
			TargetRot.Roll = 0.0f;
			P->GetCharacterMovement()->Velocity = TargetTransform.TransformVector(MyTransform.InverseTransformVector(P->GetCharacterMovement()->Velocity));
		}
		if (OtherActor->TeleportTo(AdjustedTeleportLoc, bSetRotation ? TargetRot : OtherActor->GetActorRotation()))
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), TeleportOutSound, this);
			UUTGameplayStatics::UTPlaySound(GetWorld(), TeleportInSound, OtherActor);
			//UE_LOG(UT, Warning, TEXT("%s TELEPORTED %s"), *GetName(), *OtherActor->GetName());
			if (P != NULL && P->Controller != NULL)
			{
				P->Controller->SetControlRotation(TargetViewRot);
			}
		}
		else
		{
			// UE_LOG(UT, Warning, TEXT("Teleporter %s failed to teleport %s"), *GetName(), *OtherActor->GetName());
		}

		if (P != NULL)
		{
			AUTBot* B = Cast<AUTBot>(P->Controller);
			if (B != NULL)
			{
				B->ClearMoveTarget();
			}
		}

		CurrentlyTeleportingActor = NULL;
	}
}

void AUTTeleporter::PostTeleport_Implementation(AActor* OtherActor)
{

}

#if WITH_EDITOR
void AUTTeleporter::UpdateExitArrow()
{
	if (ExitArrow != NULL)
	{
		ExitArrow->SetRelativeLocation(TeleportTarget.GetLocation());
		ExitArrow->SetRelativeRotation(TeleportTarget.Rotator());
	}
}
void AUTTeleporter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	UpdateExitArrow();
}
// below editor functions are a workaround to the editor FTransform widget not having a setting to work in world space
void AUTTeleporter::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	FTransform PrevTeleportTarget = TeleportTarget * ActorToWorld();
	Super::EditorApplyTranslation(DeltaTranslation, bAltDown, bShiftDown, bCtrlDown);
	if (bLockTeleportTarget)
	{
		TeleportTarget = PrevTeleportTarget;
		TeleportTarget.SetToRelativeTransform(ActorToWorld());
	}
	UpdateExitArrow();
}
void AUTTeleporter::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	FTransform PrevTeleportTarget = TeleportTarget * ActorToWorld();
	Super::EditorApplyRotation(DeltaRotation, bAltDown, bShiftDown, bCtrlDown);
	if (bLockTeleportTarget)
	{
		TeleportTarget = PrevTeleportTarget;
		TeleportTarget.SetToRelativeTransform(ActorToWorld());
	}
	UpdateExitArrow();
}
void AUTTeleporter::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
	FTransform PrevTeleportTarget = TeleportTarget * ActorToWorld();
	Super::EditorApplyScale(DeltaScale, PivotLocation, bAltDown, bShiftDown, bCtrlDown);
	if (bLockTeleportTarget)
	{
		TeleportTarget = PrevTeleportTarget;
		TeleportTarget.SetToRelativeTransform(ActorToWorld());
	}
	UpdateExitArrow();
}
#endif

void AUTTeleporter::AddSpecialPaths(UUTPathNode* MyNode, AUTRecastNavMesh* NavData)
{
	NavNodeRef MyPoly = NavData->UTFindNearestPoly(GetActorLocation(), GetSimpleCollisionCylinderExtent());
	FTransform WorldTransform = TeleportTarget * ActorToWorld();
	FVector AdjustedTeleportLoc = WorldTransform.GetLocation();
	NavNodeRef TargetPoly = NavData->UTFindNearestPoly(AdjustedTeleportLoc, FVector(NavData->AgentRadius, NavData->AgentRadius, NavData->AgentHeight));
	if (MyPoly != INVALID_NAVNODEREF && TargetPoly != INVALID_NAVNODEREF)
	{
		UUTPathNode* End = NavData->GetNodeFromPoly(TargetPoly);
		if (End != NULL)
		{
			UUTReachSpec_Teleport* TeleportSpec = NewObject<UUTReachSpec_Teleport>(MyNode);
			TeleportSpec->Teleporter = this;
			FCapsuleSize PathSize = NavData->GetHumanPathSize();
			FUTPathLink* NewLink = new(MyNode->Paths) FUTPathLink(MyNode, MyPoly, End, TargetPoly, TeleportSpec, PathSize.Radius, PathSize.Height, 0);
			for (NavNodeRef StartPoly : MyNode->Polys)
			{
				NewLink->Distances.Add(NavData->CalcPolyDistance(StartPoly, MyPoly) + 100);
			}
		}
	}
}