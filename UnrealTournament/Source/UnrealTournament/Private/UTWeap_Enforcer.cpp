// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "UTWeap_Enforcer.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateFiring_Enforcer.h"
#include "UTWeaponStateFiringBurstEnforcer.h"
#include "UTWeaponStateEquipping_Enforcer.h"
#include "UTWeaponStateUnequipping_Enforcer.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTImpactEffect.h"
#include "UnrealNetwork.h"
#include "UTWeaponAttachment.h"
#include "StatNames.h"

AUTWeap_Enforcer::AUTWeap_Enforcer(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	ClassicGroup = 2;
	Ammo = 20;
	MaxAmmo = 40;
	LastFireTime = 0.f;
	SpreadResetInterval = 1.f;
	SpreadIncrease = 0.03f;
	MaxSpread = 0.12f;
	BringUpTime = 0.28f;
	DualBringUpTime = 0.36f;
	PutDownTime = 0.2f;
	DualPutDownTime = 0.3f;
	StoppingPower = 30000.f;
	BaseAISelectRating = 0.4f;
	FireCount = 0;
	ImpactCount = 0;
	bDualEnforcerMode = false;
	bBecomeDual = false;
	bCanThrowWeapon = false;
	FOVOffset = FVector(0.7f, 1.f, 1.f);

	LeftMesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("LeftMesh"));
	LeftMesh->SetOnlyOwnerSee(true);
	LeftMesh->AttachParent = RootComponent;
	LeftMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	LeftMesh->bSelfShadowOnly = true;
	LeftMesh->bHiddenInGame = true;
	FirstPLeftMeshOffset = FVector(0.f);

	EnforcerEquippingState = ObjectInitializer.CreateDefaultSubobject<UUTWeaponStateEquipping_Enforcer>(this, TEXT("EnforcerEquippingState"));
	EnforcerUnequippingState = ObjectInitializer.CreateDefaultSubobject<UUTWeaponStateUnequipping_Enforcer>(this, TEXT("EnforcerUnequippingState"));

	KillStatsName = NAME_EnforcerKills;
	DeathStatsName = NAME_EnforcerDeaths;

	DisplayName = NSLOCTEXT("UTWeap_Enforcer", "DisplayName", "Enforcer");
}

void AUTWeap_Enforcer::AttachLeftMesh()
{
	if (UTOwner == NULL)
	{
		return;
	}

	if (LeftMesh != NULL && LeftMesh->SkeletalMesh != NULL)
	{
		LeftMesh->SetHiddenInGame(false);
		LeftMesh->AttachTo(UTOwner->FirstPersonMesh);
		if (Cast<APlayerController>(UTOwner->Controller) != NULL && UTOwner->IsLocallyControlled())
		{
			LeftMesh->LastRenderTime = GetWorld()->TimeSeconds;
			LeftMesh->bRecentlyRendered = true;
		}

		if (LeftBringUpAnim != NULL)
		{
			UAnimInstance* AnimInstance = LeftMesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(LeftBringUpAnim, LeftBringUpAnim->SequenceLength / EnforcerEquippingState->EquipTime);
			}
		}

		if (UTOwner != NULL && UTOwner->GetWeapon() == this && GetNetMode() != NM_DedicatedServer)
		{
			UpdateOverlays();
		}
	}
}

void AUTWeap_Enforcer::UpdateViewBob(float DeltaTime)
{
	Super::UpdateViewBob(DeltaTime);

	// if weapon is up in first person, view bob with movement
	if (LeftMesh != NULL && LeftMesh->AttachParent != NULL && UTOwner != NULL && UTOwner->GetWeapon() == this && ShouldPlay1PVisuals() && GetWeaponHand() != HAND_Hidden)
	{
		if (FirstPLeftMeshOffset.IsZero())
		{
			FirstPLeftMeshOffset = LeftMesh->GetRelativeTransform().GetLocation();
			FirstPLeftMeshRotation = LeftMesh->GetRelativeTransform().Rotator();
		}
		LeftMesh->SetRelativeLocation(FirstPLeftMeshOffset);
		LeftMesh->SetWorldLocation(LeftMesh->GetComponentLocation() + UTOwner->GetWeaponBobOffset(0.0f, this));

		LeftMesh->SetRelativeRotation(Mesh->RelativeRotation - FirstPMeshRotation + FirstPLeftMeshRotation);
	}
}

float AUTWeap_Enforcer::GetPutDownTime()
{
	return bDualEnforcerMode ? DualPutDownTime : PutDownTime;
}

float AUTWeap_Enforcer::GetBringUpTime()
{
	return bDualEnforcerMode ? DualBringUpTime : BringUpTime;
}

float AUTWeap_Enforcer::GetImpartedMomentumMag(AActor* HitActor)
{
	AUTCharacter* HitChar = Cast<AUTCharacter>(HitActor);
	return (HitChar && HitChar->GetWeapon() && HitChar->GetWeapon()->bAffectedByStoppingPower)
		? StoppingPower
		: InstantHitInfo[CurrentFireMode].Momentum;
}

void AUTWeap_Enforcer::FireShot()
{
	Super::FireShot();

	if (GetNetMode() != NM_DedicatedServer)
	{
		FireCount++;

		UUTWeaponStateFiringBurst* BurstFireMode = Cast<UUTWeaponStateFiringBurst>(FiringState[GetCurrentFireMode()]);
		if ((BurstFireMode && FireCount >= BurstFireMode->BurstSize * 2) || (!BurstFireMode && FireCount > 1))
		{
			FireCount = 0;
		}
	}

}

void AUTWeap_Enforcer::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
	// burst mode takes care of spread variation itself
	if (!Cast<UUTWeaponStateFiringBurst>(FiringState[GetCurrentFireMode()]))
	{
		float TimeSinceFired = UTOwner->GetWorld()->GetTimeSeconds() - LastFireTime;
		float SpreadScalingOverTime = FMath::Max(0.f, 1.f - (TimeSinceFired - FireInterval[GetCurrentFireMode()]) / (SpreadResetInterval - FireInterval[GetCurrentFireMode()]));
		Spread[GetCurrentFireMode()] = FMath::Min(MaxSpread, Spread[GetCurrentFireMode()] + SpreadIncrease) * SpreadScalingOverTime;
	}

	Super::FireInstantHit(bDealDamage, OutHit);
	if (UTOwner)
	{
		LastFireTime = UTOwner->GetWorld()->GetTimeSeconds();
	}
}

void AUTWeap_Enforcer::StateChanged()
{
	if (!FiringState.Contains(Cast<UUTWeaponStateFiring>(CurrentState)))
	{
		FireCount = 0;
		ImpactCount = 0;
	}

	Super::StateChanged();
}

void AUTWeap_Enforcer::PlayFiringEffects()
{
	UUTWeaponStateFiringBurstEnforcer* BurstFireMode = Cast<UUTWeaponStateFiringBurstEnforcer>(FiringState[GetCurrentFireMode()]);

	if (UTOwner != NULL)
	{
		// If they last fired the right hand enforcer
		if (!bDualEnforcerMode || (bDualEnforcerMode && !BurstFireMode && FireCount % 2 == 0) || (bDualEnforcerMode && BurstFireMode && FireCount / BurstFireMode->BurstSize == 0))
		{
			if (!BurstFireMode || BurstFireMode->CurrentShot == 0)
			{
				Super::PlayFiringEffects();
			}
			else if (ShouldPlay1PVisuals())
			{
				UTOwner->TargetEyeOffset.X = FiringViewKickback;
				// muzzle flash
				if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL && MuzzleFlash[CurrentFireMode]->Template != NULL)
				{
					// if we detect a looping particle system, then don't reactivate it
					if (!MuzzleFlash[CurrentFireMode]->bIsActive || !IsLoopingParticleSystem(MuzzleFlash[CurrentFireMode]->Template))
					{
						MuzzleFlash[CurrentFireMode]->ActivateSystem();
					}
				}
			}
		}
		else
		{
			// try and play the sound if specified
			if ((!BurstFireMode || BurstFireMode->CurrentShot == 0) && FireSound.IsValidIndex(CurrentFireMode) && FireSound[CurrentFireMode] != NULL)
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), FireSound[CurrentFireMode], UTOwner, SRT_AllButOwner);
			}
			
			if (ShouldPlay1PVisuals())
			{
				// try and play a firing animation if specified
				if ((!BurstFireMode || BurstFireMode->CurrentShot == 0) && FireAnimation.IsValidIndex(CurrentFireMode) && FireAnimation[CurrentFireMode] != NULL)
				{
					UAnimInstance* AnimInstance = LeftMesh->GetAnimInstance();
					if (AnimInstance != NULL)
					{
						AnimInstance->Montage_Play(FireAnimation[CurrentFireMode], UTOwner->GetFireRateMultiplier());
					}
				}

				UTOwner->TargetEyeOffset.X = FiringViewKickback;
				// muzzle flash
				uint8 LeftHandMuzzleFlashIndex = CurrentFireMode + 2;
				if (MuzzleFlash.IsValidIndex(LeftHandMuzzleFlashIndex) && MuzzleFlash[LeftHandMuzzleFlashIndex] != NULL && MuzzleFlash[LeftHandMuzzleFlashIndex]->Template != NULL)
				{
					// if we detect a looping particle system, then don't reactivate it
					if (!MuzzleFlash[LeftHandMuzzleFlashIndex]->bIsActive || MuzzleFlash[LeftHandMuzzleFlashIndex]->bSuppressSpawning || !IsLoopingParticleSystem(MuzzleFlash[LeftHandMuzzleFlashIndex]->Template))
					{
						MuzzleFlash[LeftHandMuzzleFlashIndex]->ActivateSystem();
					}
				}
			}
		}
	}
}


void AUTWeap_Enforcer::PlayImpactEffects(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UUTWeaponStateFiringBurst* BurstFireMode = Cast<UUTWeaponStateFiringBurst>(FiringState[GetCurrentFireMode()]);
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (bDualEnforcerMode && ((!BurstFireMode && ImpactCount % 2 != 0) || (BurstFireMode && ImpactCount / BurstFireMode->BurstSize != 0)))
		{
			// fire effects
			static FName NAME_HitLocation(TEXT("HitLocation"));
			static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));

			//TODO: This is a really ugly solution, what if somebody modifies this later
			//Is the best solution really to split out a separate MuzzleFlash too??
			uint8 LeftHandMuzzleFlashIndex = CurrentFireMode + 2;
			const FVector LeftSpawnLocation = (MuzzleFlash.IsValidIndex(LeftHandMuzzleFlashIndex) && MuzzleFlash[LeftHandMuzzleFlashIndex] != NULL) ? MuzzleFlash[LeftHandMuzzleFlashIndex]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetControlRotation().RotateVector(FireOffset);
			if (FireEffect.IsValidIndex(CurrentFireMode) && FireEffect[CurrentFireMode] != NULL)
			{
				UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[CurrentFireMode], LeftSpawnLocation, (MuzzleFlash.IsValidIndex(LeftHandMuzzleFlashIndex) && MuzzleFlash[LeftHandMuzzleFlashIndex] != NULL) ? MuzzleFlash[LeftHandMuzzleFlashIndex]->GetComponentRotation() : (TargetLoc - SpawnLocation).Rotation(), true);
				PSC->SetVectorParameter(NAME_HitLocation, TargetLoc);
				PSC->SetVectorParameter(NAME_LocalHitLocation, PSC->ComponentToWorld.InverseTransformPosition(TargetLoc));
			}
			// perhaps the muzzle flash also contains hit effect (constant beam, etc) so set the parameter on it instead
			else if (MuzzleFlash.IsValidIndex(LeftHandMuzzleFlashIndex) && MuzzleFlash[LeftHandMuzzleFlashIndex] != NULL)
			{
				MuzzleFlash[LeftHandMuzzleFlashIndex]->SetVectorParameter(NAME_HitLocation, TargetLoc);
				MuzzleFlash[LeftHandMuzzleFlashIndex]->SetVectorParameter(NAME_LocalHitLocation, MuzzleFlash[LeftHandMuzzleFlashIndex]->ComponentToWorld.InverseTransformPositionNoScale(TargetLoc));
			}

			if ((TargetLoc - LastImpactEffectLocation).Size() >= ImpactEffectSkipDistance || GetWorld()->TimeSeconds - LastImpactEffectTime >= MaxImpactEffectSkipTime)
			{
				if (ImpactEffect.IsValidIndex(CurrentFireMode) && ImpactEffect[CurrentFireMode] != NULL)
				{
					FHitResult ImpactHit = GetImpactEffectHit(UTOwner, LeftSpawnLocation, TargetLoc);
					if (!CancelImpactEffect(ImpactHit))
					{
						ImpactEffect[CurrentFireMode].GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(ImpactHit.Normal.Rotation(), ImpactHit.Location), ImpactHit.Component.Get(), NULL, UTOwner->Controller);
					}
				}
				LastImpactEffectLocation = TargetLoc;
				LastImpactEffectTime = GetWorld()->TimeSeconds;
			}
		}
		else
		{
			Super::PlayImpactEffects(TargetLoc, FireMode, SpawnLocation, SpawnRotation);
		}

		ImpactCount++;

		if ((BurstFireMode && ImpactCount >= BurstFireMode->BurstSize * 2) || (!BurstFireMode && ImpactCount > 1))
		{
			ImpactCount = 0;
		}
	}
}

bool AUTWeap_Enforcer::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	if (!bBecomeDual)
	{
		BecomeDual();
	}
	return Super::StackPickup_Implementation(ContainedInv);
}

void AUTWeap_Enforcer::BecomeDual()
{
	bBecomeDual = true;

	// pick up the second enforcer
	AttachLeftMesh();

	// the UneqippingState needs to be updated so that both guns are lowered during weapon switch
	UnequippingState = EnforcerUnequippingState;

	BaseAISelectRating = FMath::Max<float>(BaseAISelectRating, 0.6f);

	//Setup a timer to fire once the equip animation finishes
	FTimerHandle TempHandle;
	GetWorldTimerManager().SetTimer(TempHandle, this, &AUTWeap_Enforcer::DualEquipFinished, EnforcerEquippingState->EquipTime);
	MaxAmmo *= 2;
	
}

void AUTWeap_Enforcer::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTWeap_Enforcer, bBecomeDual, COND_None);
}

void AUTWeap_Enforcer::DualEquipFinished()
{
	if (!bDualEnforcerMode)
	{
		bDualEnforcerMode = true;
		FireInterval = FireIntervalDualWield;

		//Reset the FireRate timer
		if (Cast<UUTWeaponStateFiring>(CurrentState) != NULL)
		{
			((UUTWeaponStateFiring*)CurrentState)->UpdateTiming();
		}

		//Update the animation since the stance has changed
		//Change the weapon attachment
		AttachmentType = DualWieldAttachmentType;

		if (UTOwner != NULL && UTOwner->GetWeapon() == this)
		{
			GetUTOwner()->SetWeaponAttachmentClass(AttachmentType);
			if (ShouldPlay1PVisuals())
			{
				UpdateWeaponHand();
			}
		}
		
		if (Role == ROLE_Authority)
		{
			OnRep_AttachmentType();
		}
	}
}

void AUTWeap_Enforcer::BringUp(float OverflowTime)
{
	if (LeftBringUpAnim != NULL)
	{
		UAnimInstance* AnimInstance = LeftMesh->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			AnimInstance->Montage_Play(LeftBringUpAnim, LeftBringUpAnim->SequenceLength / EnforcerEquippingState->EquipTime);
		}
	}

	Super::BringUp(OverflowTime);
}

void AUTWeap_Enforcer::GotoEquippingState(float OverflowTime)
{
	GotoState(EnforcerEquippingState);
	if (CurrentState == EnforcerEquippingState)
	{
		EnforcerEquippingState->StartEquip(OverflowTime);
	}
}

void AUTWeap_Enforcer::UpdateOverlays()
{
	UpdateOverlaysShared(this, GetUTOwner(), Mesh, OverlayMesh);
	if (bBecomeDual)
	{
		UpdateOverlaysShared(this, GetUTOwner(), LeftMesh, LeftOverlayMesh);
	}
}

void AUTWeap_Enforcer::SetSkin(UMaterialInterface* NewSkin)
{
	if (LeftMesh != NULL)
	{
		if (NewSkin != NULL)
		{
			for (int32 i = 0; i < LeftMesh->GetNumMaterials(); i++)
			{
				LeftMesh->SetMaterial(i, NewSkin);
			}
		}
		else
		{
			for (int32 i = 0; i < LeftMesh->GetNumMaterials(); i++)
			{
				LeftMesh->SetMaterial(i, GetClass()->GetDefaultObject<AUTWeap_Enforcer>()->LeftMesh->GetMaterial(i));
			}
		}
	}

	Super::SetSkin(NewSkin);
}

void AUTWeap_Enforcer::AttachToOwner_Implementation()
{
	if (UTOwner == NULL)
	{
		return;
	}
	
	if (bBecomeDual && !bDualEnforcerMode)
	{
		DualEquipFinished();
	}

	// attach left mesh
	if (LeftMesh != NULL && LeftMesh->SkeletalMesh != NULL && bDualEnforcerMode)
	{
		LeftMesh->SetHiddenInGame(false);
		LeftMesh->AttachTo(UTOwner->FirstPersonMesh);
		if (ShouldPlay1PVisuals())
		{
			LeftMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose; // needed for anims to be ticked even if weapon is not currently displayed, e.g. sniper zoom
			LeftMesh->LastRenderTime = GetWorld()->TimeSeconds;
			LeftMesh->bRecentlyRendered = true;
			if (LeftOverlayMesh != NULL)
			{
				LeftOverlayMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose;
				LeftOverlayMesh->LastRenderTime = GetWorld()->TimeSeconds;
				LeftOverlayMesh->bRecentlyRendered = true;
			}
		}
	}

	if (bDualEnforcerMode)
	{
		AttachmentType = DualWieldAttachmentType;

		if (UTOwner != NULL)
		{
			GetUTOwner()->SetWeaponAttachmentClass(AttachmentType);
		}
	}

	Super::AttachToOwner_Implementation();
}

void AUTWeap_Enforcer::DetachFromOwner_Implementation()
{
	//TODO revisit this if I split the muzzle flash
	//make sure particle system really stops NOW since we're going to unregister it
	//for (int32 i = 0; i < MuzzleFlash.Num(); i++)
	//{
	//	if (MuzzleFlash[i] != NULL)
	//	{
	//		UParticleSystem* SavedTemplate = MuzzleFlash[i]->Template;
	//		MuzzleFlash[i]->DeactivateSystem();
	//		MuzzleFlash[i]->KillParticlesForced();
	//		// FIXME: KillParticlesForced() doesn't kill particles immediately for GPU particles, but the below does...
	//		MuzzleFlash[i]->SetTemplate(NULL);
	//		MuzzleFlash[i]->SetTemplate(SavedTemplate);
	//	}
	//}

	if (LeftMesh != NULL && LeftMesh->SkeletalMesh != NULL)
	{
		LeftMesh->DetachFromParent();
	}

	Super::DetachFromOwner_Implementation();
}

void AUTWeap_Enforcer::UpdateWeaponHand()
{
	Super::UpdateWeaponHand();
	if (bDualEnforcerMode)
	{
		FirstPLeftMeshOffset = FVector::ZeroVector;
		FirstPLeftMeshRotation = FRotator::ZeroRotator;
		switch (GetWeaponHand())
		{
			case HAND_Center:
				// TODO: not implemented, fallthrough
				UE_LOG(UT, Warning, TEXT("HAND_Center is not implemented yet!"));
			case HAND_Right:
				LeftMesh->SetRelativeLocationAndRotation(GetClass()->GetDefaultObject<AUTWeap_Enforcer>()->LeftMesh->RelativeLocation, GetClass()->GetDefaultObject<AUTWeap_Enforcer>()->LeftMesh->RelativeRotation);
				break;
			case HAND_Left:
			{
				// swap
				LeftMesh->SetRelativeLocationAndRotation(GetClass()->GetDefaultObject<AUTWeap_Enforcer>()->Mesh->RelativeLocation, GetClass()->GetDefaultObject<AUTWeap_Enforcer>()->Mesh->RelativeRotation);
				Mesh->SetRelativeLocationAndRotation(GetClass()->GetDefaultObject<AUTWeap_Enforcer>()->LeftMesh->RelativeLocation, GetClass()->GetDefaultObject<AUTWeap_Enforcer>()->LeftMesh->RelativeRotation);
				break;
			}
			case HAND_Hidden:
			{
				Mesh->SetRelativeLocationAndRotation(FVector(-50.0f, 20.0f, -50.0f), FRotator::ZeroRotator);
				LeftMesh->SetRelativeLocationAndRotation(FVector(-50.0f, -20.0f, -50.0f), FRotator::ZeroRotator);
				break;
			}
		}
	}
}
