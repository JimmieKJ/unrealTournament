// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTGhostFlag.h"
#include "Net/UnrealNetwork.h"
#include "UTFlagReturnTrail.h"

AUTGhostFlag::AUTGhostFlag(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	SetReplicates(true);
	NetPriority = 3.f;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	UCapsuleComponent* Root = ObjectInitializer.CreateDefaultSubobject<UCapsuleComponent>(this, TEXT("Capsule"));
	Root->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Root->bShouldUpdatePhysicsVolume = false;
	Root->Mobility = EComponentMobility::Movable;
	RootComponent = Root;

	TimerEffect = ObjectInitializer.CreateDefaultSubobject<UParticleSystemComponent>(this, TEXT("TimerEffect"));
	if (TimerEffect != NULL)
	{
		TimerEffect->SetHiddenInGame(true);
		TimerEffect->AttachParent = RootComponent;
		TimerEffect->LDMaxDrawDistance = 4000.0f;
		TimerEffect->RelativeLocation.Z = 40.0f;
		TimerEffect->Mobility = EComponentMobility::Movable;
		TimerEffect->SetCastShadow(false);
	}
	GhostMaster.MidPoints[0] = FVector(0.f);
	GhostMaster.MidPoints[1] = FVector(0.f);
	GhostMaster.MidPoints[2] = FVector(0.f);

	static ConstructorHelpers::FClassFinder<AUTFlagReturnTrail> TrailFinder(TEXT("/Game/RestrictedAssets/Effects/CTF/Blueprints/BP_FlagSplineCreator.BP_FlagSplineCreator_C"));
	TrailClass = TrailFinder.Class;
}

void AUTGhostFlag::Destroyed()
{
	Super::Destroyed();
	if (Trail)
	{
		Trail->EndTrail();
		Trail->SetLifeSpan(1.5f); //failsafe
	}
}

void AUTGhostFlag::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTGhostFlag, GhostMaster);
}

void AUTGhostFlag::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (GetNetMode() != NM_DedicatedServer && GhostMaster.MyCarriedObject != nullptr)
	{
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = *Iterator;
			if (PlayerController && PlayerController->IsLocalPlayerController() && PlayerController->GetViewTarget())
			{
				FVector Dir = GetActorLocation() - PlayerController->GetViewTarget()->GetActorLocation();
				FRotator DesiredRot = Dir.Rotation();
				DesiredRot.Yaw += 90.f;
				DesiredRot.Pitch = 0.f;
				DesiredRot.Roll = 0.f;
				SetActorRotation(DesiredRot);
				break;
			}
		}
		float TimerPosition = GhostMaster.MyCarriedObject->AutoReturnTime > 0 ? (1.0f - GhostMaster.MyCarriedObject->FlagReturnTime / GhostMaster.MyCarriedObject->AutoReturnTime) : 0.0;
		TimerEffect->SetHiddenInGame(false);
		TimerEffect->SetFloatParameter(NAME_Progress, TimerPosition);			
		TimerEffect->SetFloatParameter(NAME_RespawnTime, 60);
		if (GetWorld()->GetTimeSeconds() - TrailSpawnTime > 10.f)
		{
			OnSetCarriedObject();
		}
	}
	else
	{
		TimerEffect->SetHiddenInGame(true);
	}
}

void AUTGhostFlag::OnSetCarriedObject()
{
	if (GhostMaster.MyCarriedObject != nullptr && GetNetMode() != NM_DedicatedServer)
	{
		if (Trail != nullptr)
		{
			Trail->EndTrail();
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		Trail = GetWorld()->SpawnActor<AUTFlagReturnTrail>(TrailClass, GhostMaster.MyCarriedObject->GetActorLocation(), FRotator::ZeroRotator, Params);
		Trail->Flag = GhostMaster.MyCarriedObject;
		Trail->SetTeam(GhostMaster.MyCarriedObject->Team);
		TArray<FVector> Points;
		Points.Reserve(5);
		Points.Add(GhostMaster.MyCarriedObject->GetActorLocation());
		for (int32 i = 2; i >= 0; i--)
		{
			if (!GhostMaster.MidPoints[i].IsZero())
			{
				Points.Add(GhostMaster.MidPoints[i]);
			}
		}
		Points.Add(GetActorLocation());
		Trail->SetPoints(Points);
		TrailSpawnTime = GetWorld()->GetTimeSeconds();
	}
}

void AUTGhostFlag::SetCarriedObject(AUTCarriedObject* NewCarriedObject, const FFlagTrailPos NewPosition)
{
	GhostMaster.MyCarriedObject = NewCarriedObject;
	for (int32 i = 0; i < 3; i++)
	{
		GhostMaster.MidPoints[i] = NewPosition.MidPoints[i];
	}
	OnSetCarriedObject();
}


