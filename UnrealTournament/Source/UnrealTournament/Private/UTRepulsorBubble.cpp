// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTRepulsorBubble.h"

#include "Net/UnrealNetwork.h"
#include "UTCharacter.h"
#include "UTPlaceablePowerup.h"
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
		BubbleMesh->SetupAttachment(CollisionComp);
	}

	bAlwaysRelevant = true;
	bReplicates = true;
	LastHitByType = RepulsorLastHitType::None;
}

void AUTRepulsorBubble::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	Health = MaxHealth;

	AUTCharacter* UTOwner = Cast<AUTCharacter>(Instigator);
	if (UTOwner)
	{
		TeamNum = UTOwner->GetTeamNum();
		SetOwner(UTOwner);
	}

	CollisionComp->SetWorldScale3D(FVector(MaxRepulsorSize,MaxRepulsorSize,MaxRepulsorSize));

	//Setup up bubble MID
	if (BubbleMesh != NULL && BubbleMaterial != NULL)
	{
		MID_Bubble = UMaterialInstanceDynamic::Create(BubbleMaterial, this);
		BubbleMesh->SetMaterial(0, MID_Bubble);
	}
}

void AUTRepulsorBubble::OnViewTargetChange(AUTPlayerController* NewViewTarget)
{
	if (NewViewTarget && (NewViewTarget == Cast<AUTPlayerController>(Instigator)))
	{
		//We are spectating the owner of this bubble so fade out the center so we can see too
		MID_Bubble->SetScalarParameterValue(FName("CenterFade"), 1.0f);
	}
	else
	{
		//Un fade the center of the bubble
		MID_Bubble->SetScalarParameterValue(FName("CenterFade"), 0.0f);
	}
}

void AUTRepulsorBubble::BeginPlay()
{
	Super::BeginPlay();

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

	if (Instigator && GEngine && GEngine->GetFirstLocalPlayerController(GetWorld()))
	{
		//if we instigated this object, or are viewing the player who instigated it, we want to fade out the center so we can see
		if ((Instigator->GetController() == GEngine->GetFirstLocalPlayerController(GetWorld())) ||
			(Cast<APawn>(GEngine->GetFirstLocalPlayerController(GetWorld())->GetViewTarget()) == Instigator))
		{
			MID_Bubble->SetScalarParameterValue(FName("CenterFade"), 1.0f);
		}
		else
		{
			MID_Bubble->SetScalarParameterValue(FName("CenterFade"), 0.0f);
		}
	}

	//Set remaining time on parent item to reflect starting health
	AUTCharacter* UTChar = Cast<AUTCharacter>(Instigator);
	if (UTChar)
	{
		AUTPlaceablePowerup* ParentItem = Cast<AUTPlaceablePowerup>(UTChar->FindInventoryType(ParentInventoryItemClass));
		if (ParentItem)
		{
			ParentItem->TimeRemaining = Health;
		}
	}
}

void AUTRepulsorBubble::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AUTRepulsorBubble, TeamNum);
	DOREPLIFETIME(AUTRepulsorBubble, Health);
	DOREPLIFETIME(AUTRepulsorBubble, LastHitByType);
}

void AUTRepulsorBubble::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	FHitResult Hit;

	if (bFromSweep)
	{
		Hit = SweepResult;
	}
	else if (CollisionComp != NULL)
	{
		OtherComp->SweepComponent(Hit, GetActorLocation() - GetVelocity() * 10.0, GetActorLocation() + GetVelocity(), FQuat::Identity, CollisionComp->GetCollisionShape(), CollisionComp->bTraceComplexOnMove);
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

	if (Instigator == nullptr)
	{
		return false;
	}

	if (GetWorld())
	{
		AUTGameState* UTGS = Cast<AUTGameState>(GetWorld()->GetGameState());
		
		AUTProjectile* ActorAsProj = Cast<AUTProjectile>(OtherActor);
		if (ActorAsProj && ActorAsProj->InstigatorController)
		{
			if (ActorAsProj->InstigatorController == Instigator->GetController())
			{
				bIsOnSameTeam = true;
			}
			else
			{
				bIsOnSameTeam = UTGS->OnSameTeam(ActorAsProj->InstigatorController, Instigator);
			}
		}
		else
		{
			if (OtherActor == Instigator || OtherActor->Instigator == Instigator)
			{
				bIsOnSameTeam = true;
			}
			else
			{
				bIsOnSameTeam = UTGS->OnSameTeam(OtherActor->Instigator, Instigator);
			}
		}
	}

	return !bIsOnSameTeam;
}

void AUTRepulsorBubble::ProcessHitPlayer(AUTCharacter* OtherPlayer, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	if ( (OtherPlayer->Health > 0.f) && (!RecentlyBouncedCharacters.Contains(OtherPlayer)) )
	{
		RecentlyBouncedCharacters.Add(OtherPlayer);

		FVector LaunchAngle = (OtherPlayer->GetActorLocation() - GetActorLocation()) + KnockbackVector;
		LaunchAngle.Normalize();
		LaunchAngle *= KnockbackStrength;
		OtherPlayer->LaunchCharacter(LaunchAngle, true, true);
		
		LastHitByType = RepulsorLastHitType::Character;
		TakeDamage(HealthLostToRepulsePlayer, OtherPlayer);

		GetWorldTimerManager().SetTimer(RecentlyBouncedClearTimerHandle,this, &AUTRepulsorBubble::ClearRecentlyBouncedPlayers, RecentlyBouncedResetTime, false);
		
		OnCharacterBounce();
	}
}

void AUTRepulsorBubble::ProcessHitProjectile(AUTProjectile* OtherProj, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	LastHitByType = RepulsorLastHitType::Projectile;

	//This is before exploding / reflecting projectile so that we damage the Repulsor. Otherwise the instigator changes to us and the shield ignores the damage.
	OtherProj->DamageImpactedActor(this, CollisionComp, HitLocation, HitNormal);
	OtherProj->Instigator = Instigator;
	
	//Attempt to change instigator controller for projectile to our instigator's controller
	AUTCharacter* UTOwner = Cast<AUTCharacter>(Instigator);
	if (UTOwner)
	{
		OtherProj->InstigatorController = UTOwner->GetController();
	}

	if (bShouldReflectProjectiles)
	{
		const bool bOriginalBounceSetting = OtherProj->ProjectileMovement->bShouldBounce;
		OtherProj->ProjectileMovement->bShouldBounce = true;
		((UUTProjectileMovementComponent*)OtherProj->ProjectileMovement)->SimulateImpact(FHitResult(this, OtherProj->ProjectileMovement->UpdatedPrimitive, HitLocation, -HitNormal));
		OtherProj->ProjectileMovement->bShouldBounce = bOriginalBounceSetting;
	}
	else
	{
		//Prevent damage from being dealt to the Instigator of the Repulsor
		OtherProj->ImpactedActor = Instigator;

		//Explode projectile
		OtherProj->Explode(HitLocation, HitNormal, CollisionComp);
	}

	OnProjectileHit();
}


float AUTRepulsorBubble::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	return TakeDamage(Damage, DamageCauser);
}

float AUTRepulsorBubble::TakeDamage(float Damage, AActor* DamageCauser)
{
	if (ShouldInteractWithActor(DamageCauser))
	{
		//If an inventory item is responsible for damage and not a character/projectile
		//then it is being caused by hitscan. Otherwise this would be the instigator or projectile.
		AUTInventory* DamageFromInventory = Cast<AUTInventory>(DamageCauser);
		if (DamageFromInventory)
		{
			LastHitByType = RepulsorLastHitType::Hitscan;
			OnHitScanBlocked();
		}

		//Set remaining time on parent item to reflect new health
		AUTCharacter* UTChar = Cast<AUTCharacter>(Instigator);
		if (UTChar)
		{
			AUTPlaceablePowerup* ParentItem = Cast<AUTPlaceablePowerup>(UTChar->FindInventoryType(ParentInventoryItemClass));
			if (ParentItem)
			{
				ParentItem->TimeRemaining -= Damage;
				Health = ParentItem->TimeRemaining;

				ParentItem->ClientSetTimeRemaining(Health);
			}
		}

		if (Health < 0)
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

void AUTRepulsorBubble::OnHitScanBlocked_Implementation()
{

}

void AUTRepulsorBubble::OnCharacterBounce_Implementation()
{

}

void AUTRepulsorBubble::OnProjectileHit_Implementation()
{

}

void AUTRepulsorBubble::Reset_Implementation()
{
	Destroy();
}

void AUTRepulsorBubble::IntermissionBegin_Implementation()
{
	Destroy();
}

void AUTRepulsorBubble::OnRep_Health()
{
	//Take Damage does not fire on the receiving end for a hitscan weapon. This will
	//allow us to play hit effects when the damage from the hitscan is replicated to us.
	switch (LastHitByType)
	{
		case RepulsorLastHitType::Hitscan:
		{
			OnHitScanBlocked();
			break;
		}
	}
}