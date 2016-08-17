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
	for (int32 i = 0; i < NUM_MIDPOINTS; i++)
	{
		GhostMaster.MidPoints[i] = FVector::ZeroVector;
	}

	static ConstructorHelpers::FClassFinder<AUTFlagReturnTrail> TrailFinder(TEXT("/Game/RestrictedAssets/Effects/CTF/Blueprints/BP_FlagSplineCreator.BP_FlagSplineCreator_C"));
	TrailClass = TrailFinder.Class;
}

void AUTGhostFlag::Destroyed()
{
	Super::Destroyed();
	if (Trail)
	{
		Trail->EndTrail();
		Trail->SetLifeSpan(1.f); //failsafe
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
		float TimerPosition = GhostMaster.MyCarriedObject->AutoReturnTime > 0.f ? (1.0f - GhostMaster.MyCarriedObject->FlagReturnTime / GhostMaster.MyCarriedObject->AutoReturnTime) : 0.5f;
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
			Trail->SetLifeSpan(1.f); //failsafe
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		Trail = GetWorld()->SpawnActor<AUTFlagReturnTrail>(TrailClass, GhostMaster.FlagLocation, FRotator::ZeroRotator, Params);
		Trail->Flag = GhostMaster.MyCarriedObject;
		Trail->SetTeam(GhostMaster.MyCarriedObject->Team);
		FHitResult Hit;
		static FName NAME_GhostTrail = FName(TEXT("GhostTrail"));
		FCollisionQueryParams CollisionParms(NAME_GhostTrail, true, GhostMaster.MyCarriedObject);
		// remove unnecessary midpoints
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, GhostMaster.FlagLocation, GetActorLocation(), COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms);
		if (!bHit)
		{
			for (int32 j = 0; j < NUM_MIDPOINTS; j++)
			{
				if (!GhostMaster.MidPoints[j].IsZero())
				{
					DrawDebugSphere(GetWorld(), GhostMaster.MidPoints[j], 24.f, 12, FColor::Green, false, 10.f);
				}
				GhostMaster.MidPoints[j] = FVector::ZeroVector;
			}
		}
		else
		{
			for (int32 i = 0; i < NUM_MIDPOINTS-1; i++)
			{
				if (!GhostMaster.MidPoints[i].IsZero())
				{
					bHit = GetWorld()->LineTraceSingleByChannel(Hit, GhostMaster.FlagLocation, GhostMaster.MidPoints[i], COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms);
					if (!bHit)
					{
						// LOS to this point, remove intermediates
						for (int32 j = i+1; j < NUM_MIDPOINTS; j++)
						{
							if (!GhostMaster.MidPoints[j].IsZero())
							{
								DrawDebugSphere(GetWorld(), GhostMaster.MidPoints[j], 24.f, 12, FColor::Red, false, 10.f);
							}
							GhostMaster.MidPoints[j] = FVector::ZeroVector;
						}
						break;
					}
				}
			}
		}
		for (int32 i = NUM_MIDPOINTS - 1; i >=1 ; i--)
		{
			if (!GhostMaster.MidPoints[i].IsZero())
			{
				bHit = GetWorld()->LineTraceSingleByChannel(Hit,GhostMaster.MidPoints[i], GetActorLocation(), COLLISION_TRACE_WEAPONNOCHARACTER, CollisionParms);
				if (!bHit)
				{
					// LOS to this point, remove intermediates
					for (int32 j = i-1; j >=0; j--)
					{
						if (!GhostMaster.MidPoints[j].IsZero())
						{
							DrawDebugSphere(GetWorld(), GhostMaster.MidPoints[j], 24.f, 12, FColor::Yellow, false, 10.f);
						}
						GhostMaster.MidPoints[j] = FVector::ZeroVector;
					}
					break;
				}
			}
		}
		// @TODO FIXMESTEVE - could check midpoint to midpoint, but cost may be prohibitive

		TArray<FVector> Points;
		Points.Reserve(NUM_MIDPOINTS + 2);
		Points.Add(GhostMaster.FlagLocation);
		for (int32 i = NUM_MIDPOINTS - 1; i >= 0; i--)
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
	GhostMaster.FlagLocation = NewCarriedObject->GetActorLocation();
	for (int32 i = 0; i < NUM_MIDPOINTS; i++)
	{
		GhostMaster.MidPoints[i] = NewPosition.MidPoints[i];
	}
	OnSetCarriedObject();
}


