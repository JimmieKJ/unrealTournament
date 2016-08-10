// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTRepulsorBubble.h"

#include "Net/UnrealNetwork.h"
#include "UTCharacter.h"
#include "UTProjectile.h"
#include "UTProjectileMovementComponent.h"

AUTRepulsorBubble::AUTRepulsorBubble(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Use a sphere as a simple collision representation
	CollisionComp = ObjectInitializer.CreateOptionalDefaultSubobject<USphereComponent>(this, TEXT("SphereComp"));
	if (CollisionComp != NULL)
	{
		CollisionComp->InitSphereRadius(1.0f);
		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AUTRepulsorBubble::OnOverlapBegin);
		CollisionComp->bTraceComplexOnMove = true;
		CollisionComp->bReceivesDecals = false;
		CollisionComp->bGenerateOverlapEvents = true;
		RootComponent = CollisionComp;
	}

	BubbleMesh = ObjectInitializer.CreateOptionalDefaultSubobject<UStaticMeshComponent>(this, TEXT("BubbleMesh"));
	if (BubbleMesh != NULL)
	{
		BubbleMesh->AttachTo(CollisionComp);
	}

	SetReplicates(true);
}

void AUTRepulsorBubble::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	AUTCharacter* UTOwner = Cast<AUTCharacter>(Instigator);
	if (UTOwner)
	{
		TeamNum = UTOwner->GetTeamNum();
	}

	CollisionComp->SetWorldScale3D(FVector(MaxRepulsorSize,MaxRepulsorSize,MaxRepulsorSize));

	//Starting health set
	Health = MaxHealth;

	//Setup up bubble MID
	if (BubbleMesh != NULL && BubbleMaterial != NULL)
	{
		MID_Bubble = UMaterialInstanceDynamic::Create(BubbleMaterial, this);
		BubbleMesh->SetMaterial(0, MID_Bubble);
	}
}

void AUTRepulsorBubble::BeginPlay()
{
	if (MID_Bubble)
	{
		if (TeamNum == 0)
		{
			MID_Bubble->SetVectorParameterValue(FName("Color"), RedTeamColor);
		}
		else
		{
			MID_Bubble->SetVectorParameterValue(FName("Color"), BlueTeamColor);
		}
	}

	Super::BeginPlay();
}

void AUTRepulsorBubble::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTRepulsorBubble, TeamNum);
	DOREPLIFETIME(AUTRepulsorBubble, Health);
}

void AUTRepulsorBubble::OnOverlapBegin(AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	FHitResult Hit;

	if (bFromSweep)
	{
		Hit = SweepResult;
	}
	else if (CollisionComp != NULL)
	{
		OtherComp->SweepComponent(Hit, GetActorLocation() - GetVelocity() * 10.0, GetActorLocation() + GetVelocity(), CollisionComp->GetCollisionShape(), CollisionComp->bTraceComplexOnMove);
	}
	else
	{
		OtherComp->LineTraceComponent(Hit, GetActorLocation() - GetVelocity() * 10.0, GetActorLocation() + GetVelocity(), FCollisionQueryParams(GetClass()->GetFName(), false, this));
	}

	ProcessHit(OtherActor, OtherComp, Hit.Location, Hit.Normal);
}

void AUTRepulsorBubble::ProcessHit(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	if (ShouldInteractWithActor(OtherActor))
	{
		AUTCharacter* ActorAsUTChar = Cast<AUTCharacter>(OtherActor);
		if (ActorAsUTChar)
		{
			ProcessHitPlayer(ActorAsUTChar, OtherComp, HitLocation, HitNormal);
		}

		AUTProjectile* ActorAsProj = Cast<AUTProjectile>(OtherActor);
		if (ActorAsProj)
		{
			ProcessHitProjectile(ActorAsProj, OtherComp, HitLocation, HitNormal);
		}
	}
}

bool AUTRepulsorBubble::ShouldInteractWithActor(AActor* OtherActor)
{
	bool bIsOnSameTeam = false;

	AUTCharacter* ActorAsUTChar = Cast<AUTCharacter>(OtherActor);
	if (ActorAsUTChar)
	{
		bIsOnSameTeam = (ActorAsUTChar == Instigator) || //Always ignore the character that spawned us
						(bShouldIgnoreTeamCharacters && (ActorAsUTChar->GetTeamNum() == TeamNum) && (ActorAsUTChar->GetTeamNum() != 255)); //255 is FFA team num, in FFA everyone has the same team num, but is on their own teams
	}

	AUTProjectile* ActorAsProj = Cast<AUTProjectile>(OtherActor);
	if (ActorAsProj)
	{
		AUTCharacter* OtherProjInstigator = Cast<AUTCharacter>(ActorAsProj->Instigator);
		if (OtherProjInstigator)
		{
			bIsOnSameTeam = (OtherProjInstigator == Instigator) ||  //always ignore our own projectiles
							(bShouldIgnoreTeamProjectiles && (TeamNum == OtherProjInstigator->GetTeamNum()) && (OtherProjInstigator->GetTeamNum() != 255)); // 255 is FFA team num, in FFA everyone has the same team num, but is on their own teams
		}
	}

	AUTInventory* ActorAsInventory = Cast<AUTInventory>(OtherActor);
	if (ActorAsInventory)
	{
		if (ActorAsInventory->GetUTOwner())
		{
			const AUTCharacter* ActorAsUTChar = ActorAsInventory->GetUTOwner();
			bIsOnSameTeam = (ActorAsUTChar == Instigator) || //Always ignore the character that spawned us
							(bShouldIgnoreTeamCharacters && (ActorAsUTChar->GetTeamNum() == TeamNum) && (ActorAsUTChar->GetTeamNum() != 255)); //255 is FFA team num, in FFA everyone has the same team num, but is on their own teams

		}
	}
	
	return !bIsOnSameTeam;
}

void AUTRepulsorBubble::ProcessHitPlayer(AUTCharacter* OtherPlayer, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	if ( (OtherPlayer->Health > 0.f) && (!RecentlyBouncedCharacters.Contains(OtherPlayer)) )
	{
		RecentlyBouncedCharacters.Add(OtherPlayer);

		FVector LaunchAngle = HitNormal.IsZero() ? (OtherPlayer->GetActorLocation() - GetActorLocation()) + KnockbackVector : -HitNormal;
		LaunchAngle.Normalize();
		LaunchAngle *= KnockbackStrength;
		OtherPlayer->LaunchCharacter(LaunchAngle, true, true);
		
		TakeDamage(HealthLostToRepulsePlayer, OtherPlayer);

		GetWorldTimerManager().SetTimer(RecentlyBouncedClearTimerHandle,this, &AUTRepulsorBubble::ClearRecentlyBouncedPlayers, RecentlyBouncedResetTime, false);
	}
}

void AUTRepulsorBubble::ProcessHitProjectile(AUTProjectile* OtherProj, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	//This is before reflection of the projectile so that we damage the Repulsor. Otherwise the instigator changes to us and the shield ignores the damage.
	OtherProj->DamageImpactedActor(this, CollisionComp, HitLocation, HitNormal);

	if (bShouldReflectProjectiles)
	{
		OtherProj->Instigator = Instigator;

		const bool bOriginalBounceSetting = OtherProj->ProjectileMovement->bShouldBounce;
		OtherProj->ProjectileMovement->bShouldBounce = true;
		((UUTProjectileMovementComponent*)OtherProj->ProjectileMovement)->SimulateImpact(FHitResult(this, OtherProj->ProjectileMovement->UpdatedPrimitive, HitLocation, -HitNormal));
		OtherProj->ProjectileMovement->bShouldBounce = bOriginalBounceSetting;
	}
	else
	{
		//Prevent damage from being delt to the Instigator of the Repulsor
		OtherProj->ImpactedActor = Instigator;

		//Explode projectile
		OtherProj->Explode(HitLocation, HitNormal, CollisionComp);
	}

}


float AUTRepulsorBubble::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	return TakeDamage(Damage, DamageCauser);
}

float AUTRepulsorBubble::TakeDamage(float Damage, AActor* DamageCauser)
{
	if (ShouldInteractWithActor(DamageCauser))
	{
		Health -= Damage;

		if (Health <= 0.f)
		{
			Destroy();
		}

		return Damage;
	}

	return 0.f;
}

void AUTRepulsorBubble::ClearRecentlyBouncedPlayers()
{
	RecentlyBouncedCharacters.Empty();
}

void AUTRepulsorBubble::Destroyed()
{
	AUTCharacter* UTChar = Cast<AUTCharacter>(Instigator);
	if (UTChar)
	{
		AUTInventory* ParentItem = UTChar->FindInventoryType(ParentInventoryItemClass);
		UTChar->RemoveInventory(ParentItem);
	}

	Super::Destroyed();
}