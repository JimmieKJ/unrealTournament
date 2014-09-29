// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTTeleporter.h"
#include "AI/Navigation/NavAreas/NavArea_Default.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/NavigationOctree.h"

AUTTeleporter::AUTTeleporter(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	TriggerBox = PCIP.CreateDefaultSubobject<UBoxComponent>(this, TEXT("TriggerBox"));
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = TriggerBox;

#if WITH_EDITORONLY_DATA
	ExitArrow = PCIP.CreateEditorOnlyDefaultSubobject<UArrowComponent>(this, TEXT("ExitArrow"));
	if (ExitArrow != NULL)
	{
		ExitArrow->AttachParent = RootComponent;
		ExitArrow->ArrowColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
	}

	EntryArrow = PCIP.CreateEditorOnlyDefaultSubobject<UArrowComponent>(this, TEXT("EntryArrow"));
	if (EntryArrow != NULL)
	{
		EntryArrow->AttachParent = RootComponent;
		EntryArrow->ArrowColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
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
	return (C != NULL && C->Role == ROLE_Authority && !C->IsDead() && C->Controller != NULL);
}

// prevents re-entrancy between teleporters
static AActor* CurrentlyTeleportingActor = NULL;

void AUTTeleporter::OnOverlapBegin(AActor* OtherActor)
{
	if (OtherActor != NULL && CurrentlyTeleportingActor != OtherActor && CanTeleport(OtherActor))
	{
		CurrentlyTeleportingActor = OtherActor;

		FTransform WorldTransform = TeleportTarget * ActorToWorld();
		FVector AdjustedTeleportLoc = WorldTransform.GetLocation();
		FRotator TargetRot = WorldTransform.Rotator();
		// don't pitch or roll Pawns
		if (OtherActor->IsA(APawn::StaticClass()))
		{
			TargetRot.Pitch = 0.0f;
			TargetRot.Roll = 0.0f;
			TargetRot.Yaw = TargetRot.Yaw + FMath::UnwindDegrees(OtherActor->GetActorRotation().Yaw) - FMath::UnwindDegrees(GetActorRotation().Yaw);
		}
		if (OtherActor->TeleportTo(AdjustedTeleportLoc, bSetRotation ? TargetRot : OtherActor->GetActorRotation()))
		{
			APawn* P = Cast<APawn>(OtherActor);
			if (P != NULL && P->Controller != NULL)
			{
				P->Controller->ClientSetLocation(P->GetActorLocation(), P->GetActorRotation());
			}
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("Teleporter %s failed to teleport %s"), *GetName(), *OtherActor->GetName());
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
