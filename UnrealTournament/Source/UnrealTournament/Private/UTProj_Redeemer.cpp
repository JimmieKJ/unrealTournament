// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UnrealNetwork.h"
#include "UTProjectileMovementComponent.h"
#include "UTImpactEffect.h"
#include "UTProj_Redeemer.h"
#include "UTCTFRewardMessage.h"
#include "UTRedeemerLaunchAnnounce.h"

AUTProj_Redeemer::AUTProj_Redeemer(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	CapsuleComp = ObjectInitializer.CreateOptionalDefaultSubobject<UCapsuleComponent>(this, TEXT("CapsuleComp"));
	if (CapsuleComp != NULL)
	{
		CapsuleComp->BodyInstance.SetCollisionProfileName("ProjectileShootable");			// Collision profiles are defined in DefaultEngine.ini
		CapsuleComp->OnComponentBeginOverlap.AddDynamic(this, &AUTProjectile::OnOverlapBegin);
		CapsuleComp->bTraceComplexOnMove = true;
		CapsuleComp->InitCapsuleSize(16.f, 70.0f);
		CapsuleComp->SetRelativeRotation(FRotator(90.f, 90.f, 90.f));
		CapsuleComp->AttachParent = RootComponent;
	}

	// Movement
	ProjectileMovement->InitialSpeed = 2000.f;
	ProjectileMovement->MaxSpeed = 2000.f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	ExplosionTimings[0] = 0.5;
	ExplosionTimings[1] = 0.2;
	ExplosionTimings[2] = 0.2;
	ExplosionTimings[3] = 0.2;
	ExplosionTimings[4] = 0.2;

	ExplosionRadii[0] = 0.125f;
	ExplosionRadii[1] = 0.3f;
	ExplosionRadii[2] = 0.475f;
	ExplosionRadii[3] = 0.65f;
	ExplosionRadii[4] = 0.825f;
	ExplosionRadii[5] = 1.0f;

	CollisionFreeRadius = 1000.f;

	InitialLifeSpan = 20.0f;
	bAlwaysShootable = true;
	ProjHealth = 50;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AUTProj_Redeemer::RedeemerDenied(AController* InstigatedBy)
{
	AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GM)
	{
		APlayerState* InstigatorPS = InstigatorController ? InstigatorController->PlayerState : NULL;
		APlayerState* InstigatedbyPS = InstigatedBy ? InstigatedBy->PlayerState : NULL;
		GM->BroadcastLocalized(this, UUTCTFRewardMessage::StaticClass(), 0, InstigatedbyPS, InstigatorPS, NULL);
	}
}

float AUTProj_Redeemer::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && GS->OnSameTeam(InstigatorController, EventInstigator))
	{
		// no friendly fire
		return 0;
	}
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(EventInstigator);
	bool bUsingClientSideHits = UTPC && (UTPC->GetPredictionTime() > 0.f);
	if ((Role == ROLE_Authority) && !bUsingClientSideHits)
	{
		ProjHealth -= Damage;
		if (ProjHealth <= 0)
		{
			OnShotDown();
			RedeemerDenied(EventInstigator);
		}
	}
	else if ((Role != ROLE_Authority) && bUsingClientSideHits)
	{
		UTPC->ServerNotifyProjectileHit(this, GetActorLocation(), DamageCauser, GetWorld()->GetTimeSeconds(), Damage);
	}

	return Damage;
}

void AUTProj_Redeemer::NotifyClientSideHit(AUTPlayerController* InstigatedBy, FVector HitLocation, AActor* DamageCauser, int32 Damage)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && GS->OnSameTeam(InstigatorController, InstigatedBy))
	{
		// no friendly fire
		return;
	}

	ProjHealth -= Damage;
	if (ProjHealth <= 0)
	{
		OnShotDown();
		RedeemerDenied(InstigatedBy);
	}
}

void AUTProj_Redeemer::ExplodeTimed()
{
	if (!bExploded && Role == ROLE_Authority)
	{
		Explode(GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
	}
}

void AUTProj_Redeemer::TornOff()
{
	if (bDetonated)
	{
		OnShotDown();
	}
	else
	{
		Explode(GetActorLocation(), FVector(0.0f, 0.0f, 1.0f));
	}
}

void AUTProj_Redeemer::OnShotDown()
{
	bDetonated = true;
	if (!bExploded)
	{
		if (Role == ROLE_Authority)
		{
			bTearOff = true;
			bReplicateUTMovement = true; // so position of explosion is accurate even if flight path was a little off
		}

		// fall to ground, explode after a delay
		ProjectileMovement->ProjectileGravityScale = 1.0f;
		ProjectileMovement->MaxSpeed += 2000.0f; // make room for gravity
		ProjectileMovement->bShouldBounce = true;
		ProjectileMovement->Bounciness = 0.25f;
		SetTimerUFunc(this, FName(TEXT("ExplodeTimed")), 1.5f, false);

		if (GetNetMode() != NM_DedicatedServer)
		{
			PlayShotDownEffects();
		}
	}
}

void AUTProj_Redeemer::PlayShotDownEffects()
{
	// stop any looping audio and particles
	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);
	for (int32 i = 0; i < Components.Num(); i++)
	{
		UAudioComponent* Audio = Cast<UAudioComponent>(Components[i]);
		if (Audio != NULL)
		{
			// only stop looping (ambient) sounds - note that the just played explosion sound may be encountered here
			if (Audio->Sound != NULL && Audio->Sound->GetDuration() >= INDEFINITELY_LOOPING_DURATION)
			{
				Audio->Stop();
			}
		}
		else
		{
			UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Components[i]);
			if (PSC != NULL && IsLoopingParticleSystem(PSC->Template))
			{
				PSC->DeactivateSystem();
			}
		}
	}
}

void AUTProj_Redeemer::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	if (!bExploded)
	{
		AUTGameMode* GM = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (GM)
		{
			GM->BroadcastLocalized(this, UUTRedeemerLaunchAnnounce::StaticClass(), 3);
		}
		if (GetNetMode() != NM_DedicatedServer)
		{
			PlayShotDownEffects();
		}
		bExploded = true;
		//Guarantee detonation on projectile collision
		AUTProjectile* Projectile = Cast<AUTProjectile>(ImpactedActor);
		if (Projectile != nullptr)
		{
			if (Role == ROLE_Authority && Projectile->InstigatorController != nullptr)
			{
				RedeemerDenied(Projectile->InstigatorController);
			}
		}
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProjectileMovement->SetActive(false);

		TArray<USceneComponent*> Components;
		GetComponents<USceneComponent>(Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			Components[i]->SetHiddenInGame(true);
		}
		if (Role == ROLE_Authority)
		{
			bTearOff = true;
			bReplicateUTMovement = true; // so position of explosion is accurate even if flight path was a little off
		}

		if (ExplosionBP != NULL)
		{
			GetWorld()->SpawnActor<AActor>(ExplosionBP, FTransform(HitNormal.Rotation(), HitLocation));
			if (ExplosionEffects && ExplosionEffects.GetDefaultObject()->Audio)
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), ExplosionEffects.GetDefaultObject()->Audio, this, SRT_IfSourceNotReplicated, false, HitLocation, NULL, NULL, false, SAT_None);
			}
		}
		else if (ExplosionEffects != NULL)
		{
			ExplosionEffects.GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(HitNormal.Rotation(), HitLocation), HitComp, this, InstigatorController);
		}
		
		if (Role == ROLE_Authority)
		{
			ExplodeHitLocation = HitLocation + HitNormal;
			ExplodeMomentum = Momentum;
			ExplodeStage1();
		}
	}
}

void AUTProj_Redeemer::ExplodeStage(float RangeMultiplier)
{
	float AdjustedMomentum = ExplodeMomentum;
	FRadialDamageParams AdjustedDamageParams = GetDamageParams(NULL, ExplodeHitLocation, AdjustedMomentum);
	if (AdjustedDamageParams.OuterRadius > 0.0f)
	{
		TArray<AActor*> IgnoreActors;
		if (ImpactedActor != NULL)
		{
			IgnoreActors.Add(ImpactedActor);
		}

		StatsHitCredit = 0.f;
		UUTGameplayStatics::UTHurtRadius(this, AdjustedDamageParams.BaseDamage, AdjustedDamageParams.MinimumDamage, AdjustedMomentum, ExplodeHitLocation, RangeMultiplier * AdjustedDamageParams.InnerRadius, RangeMultiplier * AdjustedDamageParams.OuterRadius, AdjustedDamageParams.DamageFalloff,
			MyDamageType, IgnoreActors, this, InstigatorController, FFInstigatorController, FFDamageType, CollisionFreeRadius);
		if ((Role==ROLE_Authority) && (HitsStatsName != NAME_None))
		{
			AUTPlayerState* PS = InstigatorController ? Cast<AUTPlayerState>(InstigatorController->PlayerState) : NULL;
			if (PS)
			{
				PS->ModifyStatsValue(HitsStatsName, StatsHitCredit / AdjustedDamageParams.BaseDamage);
			}
		}
	}
}

void AUTProj_Redeemer::ExplodeStage1()
{
	ExplodeStage(ExplosionRadii[0]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage2, ExplosionTimings[0]);
}
void AUTProj_Redeemer::ExplodeStage2()
{
	ExplodeStage(ExplosionRadii[1]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage3, ExplosionTimings[1]);
}
void AUTProj_Redeemer::ExplodeStage3()
{
	ExplodeStage(ExplosionRadii[2]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage4, ExplosionTimings[2]);
}
void AUTProj_Redeemer::ExplodeStage4()
{
	ExplodeStage(ExplosionRadii[3]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage5, ExplosionTimings[3]);
}
void AUTProj_Redeemer::ExplodeStage5()
{
	ExplodeStage(ExplosionRadii[4]);
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTProj_Redeemer::ExplodeStage6, ExplosionTimings[4]);
}
void AUTProj_Redeemer::ExplodeStage6()
{
	ExplodeStage(ExplosionRadii[5]);
	ShutDown();
}

void AUTProj_Redeemer::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTProj_Redeemer, bDetonated, COND_None);
}

void AUTProj_Redeemer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// check outline
	// this is done in Tick() so that it handles edge cases like viewer changing teams
	if (GetNetMode() != NM_DedicatedServer)
	{
		AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GetGameState());
		IUTTeamInterface* TeamOwner = Cast<IUTTeamInterface>(Instigator);
		bool bShowOutline = false;
		if (GS != nullptr && TeamOwner != nullptr && !bExploded)
		{
			for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
			{
				if (It->PlayerController != nullptr && GS->OnSameTeam(It->PlayerController, Instigator))
				{
					// note: does not handle splitscreen
					bShowOutline = true;
					break;
				}
			}
		}
		if (bShowOutline)
		{
			if (CustomDepthMesh == nullptr)
			{
				TInlineComponentArray<UMeshComponent*> Meshes(this);
				if (Meshes.Num() > 0)
				{
					CustomDepthMesh = CreateCustomDepthOutlineMesh(Meshes[0], this);
					CustomDepthMesh->CustomDepthStencilValue = TeamOwner->GetTeamNum() + 1;
					CustomDepthMesh->RegisterComponent();
				}
			}
		}
		else if (CustomDepthMesh != nullptr)
		{
			CustomDepthMesh->DestroyComponent();
			CustomDepthMesh = nullptr;
		}
	}
}
