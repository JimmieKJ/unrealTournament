// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTProj_BioGlob.h"
#include "UTProj_BioGlobling.h"
#include "UnrealNetwork.h"

AUTProj_BioGlob::AUTProj_BioGlob(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	DamageParams.OuterRadius = 240.0f;

	MaxRestingGlobStrength = 6;
	RemainingRestTime = 0.0f;
	DamageRadiusGainFactor = 0.3f;

	InitialLifeSpan = 0.0f;

	GlobStrengthCollisionScale = 2.0f;
	ExtraRestTimePerStrength = 0.5f;

	SplashSpread = 0.8f;
}

void AUTProj_BioGlob::ProcessHit_Implementation(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FVector& HitLocation, const FVector& HitNormal)
{
	//Merge Globs
	if (Cast<AUTProj_BioShot>(OtherActor) != NULL)
	{
		//Let the goblings pass through so they dont explode the glob
		if (!OtherActor->IsA(AUTProj_BioGlobling::StaticClass()))
		{
			// increment strength by that of the other bio
			// BioShot is considered to have strength 1
			AUTProj_BioShot* OtherBio = (AUTProj_BioShot*)OtherActor;
			if (bLanded && !bExploded && !OtherBio->bLanded && !OtherBio->bExploded)
			{
				AUTProj_BioGlob* OtherGlob = Cast<AUTProj_BioGlob>(OtherBio);
				if (OtherGlob != NULL)
				{
					// transfer kill credit if incoming bio is stronger
					if (OtherGlob->GlobStrength > GlobStrength)
					{
						InstigatorController = OtherGlob->InstigatorController;
						Instigator = OtherGlob->Instigator;
					}
					MergeWithGlob(OtherGlob->GlobStrength);
				}
				else
				{
					MergeWithGlob(1);
				}
				OtherBio->ShutDown();
			}
		}
	}
	else
	{
		Super::ProcessHit_Implementation(OtherActor, OtherComp, HitLocation, HitNormal);
	}
}

void AUTProj_BioGlob::Landed(UPrimitiveComponent* HitComp)
{
	if (!bLanded)
	{
		Super::Landed(HitComp);
		SplashGloblings();
	}
}

void AUTProj_BioGlob::MergeWithGlob(int32 AdditionalGlobStrength)
{
	SetGlobStrength(GlobStrength + AdditionalGlobStrength);
	SplashGloblings();

	GrowCollision();

	//Effects
	UUTGameplayStatics::UTPlaySound(GetWorld(), MergeSound, this, ESoundReplicationType::SRT_IfSourceNotReplicated);
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MergeEffect, GetActorLocation(), SurfaceNormal.Rotation(), true);
	}
}

void AUTProj_BioGlob::SplashGloblings()
{
	if (GlobStrength > MaxRestingGlobStrength)
	{
		if (Role == ROLE_Authority && SplashProjClass != NULL)
		{
			FActorSpawnParameters Params;
			Params.Instigator = Instigator;
			Params.Owner = Instigator;

			//Adjust a bit so we don't spawn in the floor
			FVector FloorOffset = GetActorLocation() + (SurfaceNormal * 10.0f);

			//Spawn goblings for as many Glob's above MaxRestingGlobStrength
			for (uint8 g = 0; g < GlobStrength - MaxRestingGlobStrength; g++)
			{
				FVector Dir = SurfaceNormal + FMath::VRand() * SplashSpread;
				GetWorld()->SpawnActor<AUTProjectile>(SplashProjClass, FloorOffset, Dir.Rotation(), Params);
			}
		}
		SetGlobStrength(MaxRestingGlobStrength);
	}
}

void AUTProj_BioGlob::OnRep_GlobStrength()
{
	SetGlobStrength(GlobStrength);
}

void AUTProj_BioGlob::SetGlobStrength(uint8 NewStrength)
{
	uint8 OldStrength = GlobStrength;

	GlobStrength = FMath::Max(NewStrength, (uint8)1);

	RestTime = GetClass()->GetDefaultObject<AUTProj_BioGlob>()->RestTime + ExtraRestTimePerStrength * float(GlobStrength);
	if (!bLanded)
	{
		//Set the projectile speed here so the client can mirror the strength speed
		ProjectileMovement->InitialSpeed = ProjectileMovement->InitialSpeed * (0.4f + (float)NewStrength) / (1.35f * (float)NewStrength);
		ProjectileMovement->MaxSpeed = ProjectileMovement->InitialSpeed;
	}
	// don't reduce remaining time for strength lost (i.e. SplashGloblings())
	else if (GlobStrength > OldStrength)
	{
		RemainingRestTime += float(GlobStrength - OldStrength) * ExtraRestTimePerStrength;
		if (Role < ROLE_Authority)
		{
			GetWorld()->GetTimerManager().SetTimer(this, &AUTProj_BioShot::RemainingRestTimer, RemainingRestTime, false);
		}
	}

	//Increase The collision of the flying Glob if over a certain strength
	if (!bLanded && GlobStrength > 4)
	{
		float DefaultRadius = Cast<AUTProjectile>(StaticClass()->GetDefaultObject())->CollisionComp->GetUnscaledSphereRadius();
		CollisionComp->SetSphereRadius(DefaultRadius + (GlobStrength * GlobStrengthCollisionScale));
	}

	// set different damagetype for charged shots
	if (GlobStrength > 1)
	{
		MyDamageType = ChargedDamageType;
	}

	DamageParams = GetClass()->GetDefaultObject<AUTProj_BioGlob>()->DamageParams;
	DamageParams.BaseDamage *= GlobStrength;
	DamageParams.OuterRadius *= 1.0 + (DamageRadiusGainFactor * float(GlobStrength - 1));
	Momentum = GetClass()->GetDefaultObject<AUTProj_BioGlob>()->Momentum * FMath::Sqrt(GlobStrength);

	//Update any effects
	OnSetGlobStrength();
}
void AUTProj_BioGlob::OnSetGlobStrength_Implementation()
{
}

void AUTProj_BioGlob::GrowCollision()
{
	CollisionComp->SetSphereRadius(FloorCollisionRadius + (GlobStrength * GlobStrengthCollisionScale), false);
}

void AUTProj_BioGlob::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTProj_BioGlob, GlobStrength, COND_InitialOnly);
}