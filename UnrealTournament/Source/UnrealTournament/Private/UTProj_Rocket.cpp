// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_Rocket.h"
#include "UnrealNetwork.h"
#include "UTRewardMessage.h"
#include "StatNames.h"

AUTProj_Rocket::AUTProj_Rocket(const class FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	DamageParams.BaseDamage = 100;
	DamageParams.OuterRadius = 360.f;  
	DamageParams.InnerRadius = 15.f; 
	DamageParams.MinimumDamage = 20.f;
	Momentum = 140000.0f;
	InitialLifeSpan = 10.f;
	ProjectileMovement->InitialSpeed = 2700.f;
	ProjectileMovement->MaxSpeed = 2700.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;

	PrimaryActorTick.bCanEverTick = true;
	AdjustmentSpeed = 5000.0f;
	bLeadTarget = true;
	bRocketTeamSet = false;
}

void AUTProj_Rocket::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (TargetActor != NULL)
	{
		FVector WantedDir = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		if (bLeadTarget)
		{
			WantedDir += TargetActor->GetVelocity() * WantedDir.Size() / ProjectileMovement->MaxSpeed;
		}

		ProjectileMovement->Velocity += WantedDir * AdjustmentSpeed * DeltaTime;
		ProjectileMovement->Velocity = ProjectileMovement->Velocity.GetSafeNormal() * ProjectileMovement->MaxSpeed;

		//If the rocket has passed the target stop following
		if (FVector::DotProduct(WantedDir, ProjectileMovement->Velocity) < 0.0f)
		{
			TargetActor = NULL;
		}
		else if (MeshMI && (int32(5.f*GetWorld()->GetTimeSeconds()) != int32(5.f*(GetWorld()->GetTimeSeconds()-DeltaTime))))
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(Instigator);
			if (UTChar && (UTChar->GetTeamColor() != FLinearColor::White))
			{
				static FName NAME_GunGlowsColor(TEXT("Gun_Glows_Color"));
				FLinearColor NewColor = (int32(5.f*GetWorld()->GetTimeSeconds()) % 2 == 0) ? UTChar->GetTeamColor() : FLinearColor::White;
				MeshMI->SetVectorParameterValue(NAME_GunGlowsColor, NewColor);
				bRocketTeamSet = (NewColor != FLinearColor::White);
			}
		}
		else if (!bRocketTeamSet && Instigator)
		{
			OnRep_Instigator();
		}
	}

}

void AUTProj_Rocket::OnRep_Instigator()
{
	Super::OnRep_Instigator();
	AUTCharacter* UTChar = Cast<AUTCharacter>(Instigator);
	if (UTChar && (UTChar->GetTeamColor() != FLinearColor::White))
	{
		bRocketTeamSet = true;
		TArray<UStaticMeshComponent*> MeshComponents;
		GetComponents<UStaticMeshComponent>(MeshComponents);
		static FName NAME_GunGlowsColor(TEXT("Gun_Glows_Color"));
		UStaticMeshComponent* GlowMesh = (MeshComponents.Num() > 0) ? MeshComponents[0] : nullptr;
		MeshMI = GlowMesh ? GlowMesh->CreateAndSetMaterialInstanceDynamic(1) : nullptr;
		if (MeshMI != nullptr)
		{
			MeshMI->SetVectorParameterValue(NAME_GunGlowsColor, UTChar->GetTeamColor());
		}
	}
}

void AUTProj_Rocket::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AUTProj_Rocket, TargetActor);
}

void AUTProj_Rocket::DamageImpactedActor_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	AUTCharacter* HitCharacter = Cast<AUTCharacter>(OtherActor);
	bPendingSpecialReward = (HitCharacter && AirRocketRewardClass && (HitCharacter->Health > 0) && HitCharacter->GetCharacterMovement() != NULL && (HitCharacter->GetCharacterMovement()->MovementMode == MOVE_Falling) && (GetWorld()->GetTimeSeconds() - HitCharacter->FallingStartTime > 0.3f));

	Super::DamageImpactedActor_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
	if (bPendingSpecialReward && HitCharacter && (HitCharacter->Health <= 0))
	{
		// Air Rocket reward
		AUTPlayerController* PC = Cast<AUTPlayerController>(InstigatorController);
		if (PC && PC->UTPlayerState)
		{
			int32 AirRoxCount = 0;
			PC->UTPlayerState->ModifyStatsValue(NAME_AirRox, 1);
			PC->UTPlayerState->AddCoolFactorMinorEvent();
			AirRoxCount = PC->UTPlayerState->GetStatsValue(NAME_AirRox);
			PC->SendPersonalMessage(AirRocketRewardClass, AirRoxCount, PC->UTPlayerState, HitCharacter->PlayerState);
		}
	}
	bPendingSpecialReward = false;
}

void AUTProj_Rocket::Explode_Implementation(const FVector& HitLocation, const FVector& HitNormal, UPrimitiveComponent* HitComp)
{
	AUTCharacter* HitCharacter = Cast<AUTCharacter>(ImpactedActor);
	bool bFollowersTrack = (!bExploded && (Role == ROLE_Authority) && (FollowerRockets.Num() > 0) && HitCharacter);

	Super::Explode_Implementation(HitLocation, HitNormal, HitComp);
	if (bFollowersTrack && HitCharacter && (HitCharacter->Health > 0))
	{
		for (int32 i = 0; i < FollowerRockets.Num(); i++)
		{
			if (FollowerRockets[i] && !FollowerRockets[i]->IsPendingKillPending())
			{
				FollowerRockets[i]->TargetActor = HitCharacter;
				FollowerRockets[i]->AdjustmentSpeed = 24000.f;
			}
		}
	}
}
