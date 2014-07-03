// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTCharacterMovement.h"
#include "UTProjectile.h"
#include "UTWeaponAttachment.h"
#include "UnrealNetwork.h"
#include "UTDmgType_Suicide.h"
#include "UTDmgType_Fell.h"

//////////////////////////////////////////////////////////////////////////
// AUTCharacter

AUTCharacter::AUTCharacter(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP.SetDefaultSubobjectClass<UUTCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	CapsuleComponent->InitCapsuleSize(42.f, 96.0f);

	// Create a CameraComponent	
	CharacterCameraComponent = PCIP.CreateDefaultSubobject<UCameraComponent>(this, TEXT("FirstPersonCamera"));
	CharacterCameraComponent->AttachParent = CapsuleComponent;
	CharacterCameraComponent->RelativeLocation = FVector(0, 0, 64.f); // Position the camera

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	FirstPersonMesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("CharacterMesh1P"));
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->AttachParent = CharacterCameraComponent;
	FirstPersonMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;

	Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;

	HealthMax = 100;
	SuperHealthMax = 199;
	DamageScaling = 1.0f;
	FireRateMultiplier = 1.0f;
	bSpawnProtectionEligible = true;
	MaxSafeFallSpeed = 2400.0f;
	FallingDamageFactor = 100.0f;
	CrushingDamageFactor = 20.0f;
	HeadScale = 1.0f;
	HeadRadius = 9.0f;
	HeadHeight = 5.0f;
	HeadBone = FName(TEXT("b_Head"));

	BobTime = 0.f;
	WeaponBobMagnitude = FVector(0.f, 0.0010f, 0.0007f);
	WeaponJumpBob = -7.f;
	WeaponLandBob = 18.f;
	WeaponBreathingBobRate = 0.2f;
	WeaponRunningBobRate = 0.8f;
	WeaponJumpBobInterpRate = 10.f;
	WeaponLandBobDecayRate = 12.f;
	EyeOffset = FVector(0.f, 0.f, 0.f);
	TargetEyeOffset = EyeOffset;
	EyeOffsetInterpRate = 12.f;
	EyeOffsetDecayRate = 12.f;
	EyeOffsetLandBob = -160.f;

	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AUTCharacter::SetMeshVisibility(bool bThirdPersonView)
{
	FirstPersonMesh->SetOwnerNoSee(bThirdPersonView);

	Mesh->SetVisibility(bThirdPersonView);
	Mesh->SetOwnerNoSee(!bThirdPersonView);
}

void AUTCharacter::BeginPlay()
{
	if (Health == 0 && Role == ROLE_Authority)
	{
		Health = HealthMax;
	}
	BaseEyeHeight = CharacterCameraComponent->RelativeLocation.Z;
	if (CharacterCameraComponent->RelativeLocation.Size2D() > 0.0f)
	{
		UE_LOG(UT, Warning, TEXT("%s: CameraComponent shouldn't have X/Y translation!"), *GetName());
	}
	Super::BeginPlay();
}

void AUTCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	BodyMI = Mesh->CreateAndSetMaterialInstanceDynamic(0);
}

void AUTCharacter::RecalculateBaseEyeHeight()
{
	BaseEyeHeight = CharacterCameraComponent->RelativeLocation.Z;
}

void AUTCharacter::Restart()
{
	Super::Restart();
	ClearJumpInput();

	// make sure equipped weapon state is synchronized
	if (IsLocallyControlled())
	{
		if (Weapon != NULL)
		{
			SwitchWeapon(Weapon);
		}
		else
		{
			SwitchToBestWeapon();
		}
	}
}

void AUTCharacter::DeactivateSpawnProtection()
{
	bSpawnProtectionEligible = false;
	// TODO: visual effect
}

void AUTCharacter::SetHeadScale(float NewHeadScale)
{
	HeadScale = NewHeadScale;
	if (GetNetMode() != NM_DedicatedServer)
	{
		HeadScaleUpdated();
	}
}

void AUTCharacter::HeadScaleUpdated()
{
	// TODO
}

bool AUTCharacter::IsHeadShot(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, bool bConsumeArmor)
{
	// force mesh update if necessary
	if (!Mesh->ShouldTickPose())
	{
		Mesh->TickAnimation(0.0f);
		Mesh->RefreshBoneTransforms();
		Mesh->UpdateComponentToWorld();
	}
	FVector HeadLocation = Mesh->GetSocketLocation(HeadBone) + FVector(0.0f, 0.0f, HeadHeight);
	bool bHeadShot = FMath::PointDistToLine(HeadLocation, ShotDirection, HitLocation) < HeadRadius * HeadScale * WeaponHeadScaling;
	if (bHeadShot)
	{
		// check for inventory items that prevent headshots
		for (AUTInventory* Inv = GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			if (Inv->bCallDamageEvents && Inv->PreventHeadShot(HitLocation, ShotDirection, WeaponHeadScaling, bConsumeArmor))
			{
				bHeadShot = false;
				break;
			}
		}
	}
	return bHeadShot;
}

FVector AUTCharacter::GetWeaponBobOffset(float DeltaTime, AUTWeapon* MyWeapon)
{
	FRotationMatrix RotMatrix = FRotationMatrix(GetViewRotation());
	FVector X = RotMatrix.GetScaledAxis(EAxis::X);
	FVector Y = RotMatrix.GetScaledAxis(EAxis::Y);
	FVector Z = RotMatrix.GetScaledAxis(EAxis::Z);

	float InterpTime = FMath::Min(1.f, WeaponJumpBobInterpRate*DeltaTime);
	if (!CharacterMovement || CharacterMovement->IsFalling() || !MyWeapon)
	{
		BobTime = 0.f;
		CurrentWeaponBob = (1.f - InterpTime)*CurrentWeaponBob + InterpTime*CurrentWeaponBob;
	}
	else
	{
		float Speed = CharacterMovement->Velocity.Size();
		BobTime += (Speed > 20.f)
			? DeltaTime * (WeaponBreathingBobRate + WeaponRunningBobRate*Speed/CharacterMovement->MaxWalkSpeed)
			: WeaponBreathingBobRate*DeltaTime;
		DesiredJumpBob *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
		CurrentWeaponBob.X = 0.f;
		CurrentWeaponBob.Y = MyWeapon->WeaponBobScaling * WeaponBobMagnitude.Y*Speed * FMath::Sin(8.f*BobTime);
		CurrentWeaponBob.Z = MyWeapon->WeaponBobScaling * WeaponBobMagnitude.Z*Speed * FMath::Sin(16.f*BobTime);

		// play footstep sounds when weapon changes bob direction if walking
		if (CharacterMovement->MovementMode == MOVE_Walking && Speed > 10.0f && !bIsCrouched && (FMath::FloorToInt(0.5f + 8.f*BobTime/PI) != FMath::FloorToInt(0.5f + 8.f*(BobTime+DeltaTime)/PI)))
		{
			PlayFootstep((LastFoot + 1) & 1);
		}
	}
	CurrentJumpBob = (1.f - InterpTime)*CurrentJumpBob + InterpTime*DesiredJumpBob;
	return CurrentWeaponBob.Y*Y + (CurrentWeaponBob.Z + CurrentJumpBob)*Z + EyeOffset;
}

float AUTCharacter::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser))
	{
		return 0.f;
	}
	else if (Damage < 0.0f)
	{
		UE_LOG(UT, Warning, TEXT("TakeDamage() called with damage %i of type %s... use HealDamage() to add health"), int32(Damage), *DamageEvent.DamageTypeClass->GetName());
		return 0.0f;
	}
	else
	{
		const UDamageType* const DamageTypeCDO = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
		const UUTDamageType* const UTDamageTypeCDO = Cast<UUTDamageType>(DamageTypeCDO); // warning: may be NULL

		int32 ResultDamage = FMath::TruncToInt(Damage);
		FVector ResultMomentum = UTGetDamageMomentum(DamageEvent, this, EventInstigator);
		if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
		{
			bool bScaleMomentum = !DamageEvent.IsOfType(FUTRadialDamageEvent::ClassID) || ((const FUTRadialDamageEvent&)DamageEvent).bScaleMomentum;
			if (Damage == 0.0f)
			{
				if (bScaleMomentum)
				{
					// use fake 1.0 damage so we can use the damage scaling code to scale momentum
					ResultMomentum *= InternalTakeRadialDamage(1.0f, (const FRadialDamageEvent&)DamageEvent, EventInstigator, DamageCauser);
				}
			}
			else
			{
				float AdjustedDamage = InternalTakeRadialDamage(Damage, (const FRadialDamageEvent&)DamageEvent, EventInstigator, DamageCauser);
				if (bScaleMomentum)
				{
					ResultMomentum *= AdjustedDamage / Damage;
				}
				ResultDamage = FMath::TruncToInt(AdjustedDamage);
			}
		}
		if (!IsDead())
		{
			// note that we split the gametype query out so that it's always in a consistent place
			AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (Game != NULL)
			{
				Game->ModifyDamage(ResultDamage, ResultMomentum, this, EventInstigator, DamageEvent, DamageCauser);
			}
			ModifyDamageTaken(ResultDamage, ResultMomentum, DamageEvent, EventInstigator, DamageCauser);

			if (ResultDamage > 0 || !ResultMomentum.IsZero())
			{
				if (EventInstigator != NULL && EventInstigator != Controller)
				{
					LastHitBy = EventInstigator;
				}

				if (ResultDamage > 0)
				{
					// this is partially copied from AActor::TakeDamage() (just the calls to the various delegates and K2 notifications)

					float ActualDamage = float(ResultDamage); // engine hooks want float
					// generic damage notifications sent for any damage
					ReceiveAnyDamage(ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
					OnTakeAnyDamage.Broadcast(ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
					if (EventInstigator != NULL)
					{
						EventInstigator->InstigatedAnyDamage(ActualDamage, DamageTypeCDO, this, DamageCauser);
					}
					if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
					{
						// point damage event, pass off to helper function
						FPointDamageEvent* const PointDamageEvent = (FPointDamageEvent*)&DamageEvent;

						// K2 notification for this actor
						if (ActualDamage != 0.f)
						{
							ReceivePointDamage(ActualDamage, DamageTypeCDO, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.ImpactNormal, PointDamageEvent->HitInfo.Component.Get(), PointDamageEvent->HitInfo.BoneName, PointDamageEvent->ShotDirection, EventInstigator, DamageCauser);
							OnTakePointDamage.Broadcast(ActualDamage, EventInstigator, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.Component.Get(), PointDamageEvent->HitInfo.BoneName, PointDamageEvent->ShotDirection, DamageTypeCDO, DamageCauser);
						}
					}
					else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
					{
						// radial damage event, pass off to helper function
						FRadialDamageEvent* const RadialDamageEvent = (FRadialDamageEvent*)&DamageEvent;

						// K2 notification for this actor
						if (ActualDamage != 0.f)
						{
							FHitResult const& Hit = (RadialDamageEvent->ComponentHits.Num() > 0) ? RadialDamageEvent->ComponentHits[0] : FHitResult();
							ReceiveRadialDamage(ActualDamage, DamageTypeCDO, RadialDamageEvent->Origin, Hit, EventInstigator, DamageCauser);
						}
					}
				}
			}

			Health -= ResultDamage;
			if (UTDamageTypeCDO != NULL && UTDamageTypeCDO->bForceZMomentum)
			{
				ResultMomentum.Z = FMath::Max<float>(ResultMomentum.Z, 0.4f * ResultMomentum.Size());
			}
			// if Z impulse is low enough and currently walking, remove Z impulse to prevent switch to falling physics, preventing lockdown effects
			else if (CharacterMovement->MovementMode == MOVE_Walking && ResultMomentum.Z < ResultMomentum.Size() * 0.1f)
			{
				ResultMomentum.Z = 0.0f;
			}
			CharacterMovement->AddMomentum(ResultMomentum, false);
			NotifyTakeHit(EventInstigator, ResultDamage, ResultMomentum, DamageEvent);
			SetLastTakeHitInfo(ResultDamage, ResultMomentum, DamageEvent);
			if (Health <= 0)
			{
				Died(EventInstigator, DamageEvent);
			}
		}
		else
		{
			FVector HitLocation = Mesh->GetComponentLocation();
			if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
			{
				HitLocation = ((const FPointDamageEvent&)DamageEvent).HitInfo.Location;
			}
			else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
			{
				const FRadialDamageEvent& RadialEvent = (const FRadialDamageEvent&)DamageEvent;
				if (RadialEvent.ComponentHits.Num() > 0)
				{
					HitLocation = RadialEvent.ComponentHits[0].Location;
				}
			}
			Mesh->AddImpulseAtLocation(ResultMomentum, HitLocation);
		}
	
		return float(ResultDamage);
	}
}

void AUTCharacter::ModifyDamageTaken_Implementation(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// check for caused modifiers on instigator
	AUTCharacter* InstigatorChar = NULL;
	if (DamageCauser != NULL)
	{
		InstigatorChar = Cast<AUTCharacter>(DamageCauser->Instigator);
	}
	if (InstigatorChar == NULL && EventInstigator != NULL)
	{
		InstigatorChar = Cast<AUTCharacter>(EventInstigator->GetPawn());
	}
	if (InstigatorChar != NULL && !InstigatorChar->IsDead())
	{
		InstigatorChar->ModifyDamageCaused(Damage, Momentum, DamageEvent, this, EventInstigator, DamageCauser);
	}
	// check inventory
	for (AUTInventory* Inv = InventoryList; Inv != NULL; Inv = Inv->GetNext())
	{
		if (Inv->bCallDamageEvents)
		{
			Inv->ModifyDamageTaken(Damage, Momentum, DamageEvent, EventInstigator, DamageCauser);
		}
	}
}
void AUTCharacter::ModifyDamageCaused_Implementation(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AActor* Victim, AController* EventInstigator, AActor* DamageCauser)
{
	Damage *= DamageScaling;
}

void AUTCharacter::SetLastTakeHitInfo(int32 Damage, const FVector& Momentum, const FDamageEvent& DamageEvent)
{
	LastTakeHitInfo.Damage = Damage;
	LastTakeHitInfo.DamageType = DamageEvent.DamageTypeClass;
	LastTakeHitInfo.Momentum = Momentum;

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		LastTakeHitInfo.RelHitLocation = ((FPointDamageEvent*)&DamageEvent)->HitInfo.Location - GetActorLocation();
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && ((FRadialDamageEvent*)&DamageEvent)->ComponentHits.Num() > 0)
	{
		LastTakeHitInfo.RelHitLocation = ((FRadialDamageEvent*)&DamageEvent)->ComponentHits[0].Location - GetActorLocation();
	}

	LastTakeHitTime = GetWorld()->TimeSeconds;
			
	PlayTakeHitEffects();
}

void AUTCharacter::PlayTakeHitEffects_Implementation()
{
	if (GetNetMode() != NM_DedicatedServer && LastTakeHitInfo.Damage > 0)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodEffect, LastTakeHitInfo.RelHitLocation + GetActorLocation(), LastTakeHitInfo.RelHitLocation.Rotation());
	}
}

void AUTCharacter::NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent)
{
	if (Role == ROLE_Authority)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
		if (PC != NULL)
		{
			PC->NotifyTakeHit(InstigatedBy, Damage, Momentum, DamageEvent);
		}
		else
		{
			// TODO: bots
		}
	}
}

bool AUTCharacter::Died(AController* EventInstigator, const FDamageEvent& DamageEvent)
{
	if (Role < ROLE_Authority || IsDead())
	{
		// can't kill pawns on client
		// can't kill pawns that are already dead :)
		return false;
	}
	else
	{
		// if this is an environmental death then refer to the previous killer so that they receive credit (knocked into lava pits, etc)
		if (DamageEvent.DamageTypeClass != NULL && DamageEvent.DamageTypeClass.GetDefaultObject()->bCausedByWorld && (EventInstigator == NULL || EventInstigator == Controller) && LastHitBy != NULL)
		{
			EventInstigator = LastHitBy;
		}

		// TODO: GameInfo::PreventDeath()

		bTearOff = true; // important to set this as early as possible so IsDead() returns true

		GetWorld()->GetAuthGameMode<AUTGameMode>()->Killed(EventInstigator, (Controller != NULL) ? Controller : Cast<AController>(GetOwner()), this, DamageEvent.DamageTypeClass);

		Health = FMath::Min<int32>(Health, 0);
		if (Controller != NULL)
		{
			Controller->PawnPendingDestroy(this);
		}

		PlayDying();

		return true;
	}
}

void AUTCharacter::PlayDying()
{
	SetAmbientSound(NULL);

	// TODO: damagetype effects, etc
	CharacterMovement->ApplyAccumulatedMomentum(0.0f);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetAllBodiesPhysicsBlendWeight(1.0f);
	Mesh->DetachFromParent(true);
	RootComponent = Mesh;
	CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapsuleComponent->DetachFromParent(false);
	CapsuleComponent->AttachTo(Mesh);
	SetLifeSpan(10.0f); // TODO: destroy early if hidden, et al

	Mesh->SetAllPhysicsLinearVelocity(GetMovementComponent()->Velocity * FVector(1.0f, 1.0f, 0.5f), false); // gravity doesn't seem to be the same magnitude for ragdolls...
}

void AUTCharacter::Destroyed()
{
	Super::Destroyed();

	DiscardAllInventory();
	if (WeaponAttachment != NULL)
	{
		WeaponAttachment->Destroy();
		WeaponAttachment = NULL;
	}
}

void AUTCharacter::SetAmbientSound(USoundBase* NewAmbientSound, bool bClear)
{
	if (bClear)
	{
		if (NewAmbientSound == AmbientSound)
		{
			AmbientSound = NULL;
		}
	}
	else
	{
		AmbientSound = NewAmbientSound;
	}
	AmbientSoundUpdated();
}
void AUTCharacter::AmbientSoundUpdated()
{
	if (AmbientSound == NULL)
	{
		if (AmbientSoundComp != NULL)
		{
			AmbientSoundComp->Stop();
		}
	}
	else
	{
		if (AmbientSoundComp == NULL)
		{
			AmbientSoundComp = NewObject<UAudioComponent>(this);
			AmbientSoundComp->bAutoDestroy = false;
			AmbientSoundComp->bAutoActivate = false;
			AmbientSoundComp->AttachTo(RootComponent);
			AmbientSoundComp->RegisterComponent();
		}
		if (AmbientSoundComp->Sound != AmbientSound)
		{
			AmbientSoundComp->SetSound(AmbientSound);
		}
		if (!AmbientSoundComp->IsPlaying())
		{
			AmbientSoundComp->Play();
		}
	}
}

void AUTCharacter::StartFire(uint8 FireModeNum)
{
	if (!IsLocallyControlled())
	{
		UE_LOG(UT, Warning, TEXT("StartFire() can only be called on the owning client"));
	}
	else if (Weapon != NULL)
	{
		Weapon->StartFire(FireModeNum);
	}
}

void AUTCharacter::StopFire(uint8 FireModeNum)
{
	if (!IsLocallyControlled())
	{
		UE_LOG(UT, Warning, TEXT("StopFire() can only be called on the owning client"));
	}
	else if (Weapon != NULL)
	{
		Weapon->StopFire(FireModeNum);
	}
}

bool AUTCharacter::IsTriggerDown(uint8 FireMode)
{
	return IsPendingFire(FireMode);
}

void AUTCharacter::SetFlashLocation(const FVector& InFlashLoc, uint8 InFireMode)
{
	// make sure two consecutive shots don't set the same FlashLocation as that will prevent replication and thus clients won't see the shot
	FlashLocation = ((FlashLocation - InFlashLoc).SizeSquared() >= 1.0f) ? InFlashLoc : (InFlashLoc + FVector(0.0f, 0.0f, 1.0f));
	// we reserve the zero vector to stop firing, so make sure we aren't setting a value that would replicate that way
	if (FlashLocation.SizeSquared() < 1.0f)
	{
		FlashLocation.Z += 1.1f;
	}
	FireMode = InFireMode;
	FiringInfoUpdated();
}
void AUTCharacter::IncrementFlashCount(uint8 InFireMode)
{
	FlashCount++;
	// we reserve zero for not firing; make sure we don't set that
	if (FlashCount == 0)
	{
		FlashCount++;
	}
	FireMode = InFireMode;
	FiringInfoUpdated();
}
void AUTCharacter::ClearFiringInfo()
{
	FlashLocation = FVector::ZeroVector;
	FlashCount = 0;
	FiringInfoUpdated();
}
void AUTCharacter::FiringInfoUpdated()
{
	if (Weapon != NULL && IsLocallyControlled() && Cast<APlayerController>(Controller) != NULL)  // and in first person?
	{
		if (!FlashLocation.IsZero())
		{
			Weapon->PlayImpactEffects(FlashLocation);
		}
	}
	else if (WeaponAttachment != NULL)
	{
		if (FlashCount != 0 || !FlashLocation.IsZero())
		{
			WeaponAttachment->PlayFiringEffects();
		}
		else
		{
			WeaponAttachment->StopFiringEffects();
		}
	}
}

void AUTCharacter::AddAmmo(const FStoredAmmo& AmmoToAdd)
{
	AUTWeapon* ExistingWeapon = FindInventoryType<AUTWeapon>(AmmoToAdd.Type, true);
	if (ExistingWeapon != NULL)
	{
		ExistingWeapon->AddAmmo(AmmoToAdd.Amount);
	}
	else
	{
		for (int32 i = 0; i < SavedAmmo.Num(); i++)
		{
			if (SavedAmmo[i].Type == AmmoToAdd.Type)
			{
				// note that we're not clamping to max here, the weapon does so when the ammo is applied to it
				SavedAmmo[i].Amount += AmmoToAdd.Amount;
				if (SavedAmmo[i].Amount <= 0)
				{
					SavedAmmo.RemoveAt(i);
				}
				return;
			}
		}
		if (AmmoToAdd.Amount > 0)
		{
			new(SavedAmmo)FStoredAmmo(AmmoToAdd);
		}
	}
}

bool AUTCharacter::HasMaxAmmo(TSubclassOf<AUTWeapon> Type)
{
	int32 Amount = 0;
	for (int32 i = 0; i < SavedAmmo.Num(); i++)
	{
		if (SavedAmmo[i].Type == Type)
		{
			Amount += SavedAmmo[i].Amount;
		}
	}
	AUTWeapon* Weapon = FindInventoryType<AUTWeapon>(Type, true);
	if (Weapon != NULL)
	{
		Amount += Weapon->Ammo;
		return Amount >= Weapon->MaxAmmo;
	}
	else
	{
		return Amount >= Type.GetDefaultObject()->MaxAmmo;
	}
}

AUTInventory* AUTCharacter::K2_FindInventoryType(TSubclassOf<AUTInventory> Type, bool bExactClass)
{
	for (AUTInventory* Inv = InventoryList; Inv != NULL; Inv = Inv->GetNext())
	{
		if (bExactClass ? (Inv->GetClass() == Type) : Inv->IsA(Type))
		{
			return Inv;
		}
	}
	return NULL;
}

void AUTCharacter::AddInventory(AUTInventory* InvToAdd, bool bAutoActivate)
{
	if (InvToAdd != NULL)
	{
		if (InventoryList == NULL)
		{
			InventoryList = InvToAdd;
		}
		else if (InventoryList == InvToAdd)
		{
			UE_LOG(UT, Warning, TEXT("AddInventory: %s already in %s's inventory!"), *InvToAdd->GetName(), *GetName());
		}
		else
		{
			AUTInventory* Last = InventoryList;
			while (Last->NextInventory != NULL)
			{
				// avoid recursion
				if (Last->NextInventory == InvToAdd)
				{
					UE_LOG(UT, Warning, TEXT("AddInventory: %s already in %s's inventory!"), *InvToAdd->GetName(), *GetName());
					return;
				}
				Last = Last->NextInventory;
			}
			Last->NextInventory = InvToAdd;
		}
		InvToAdd->GivenTo(this, bAutoActivate);
	}
}

void AUTCharacter::RemoveInventory(AUTInventory* InvToRemove)
{
	if (InvToRemove != NULL && InventoryList != NULL)
	{
		if (InvToRemove == InventoryList)
		{
			InventoryList = InventoryList->NextInventory;
		}
		else
		{
			for (AUTInventory* TestInv = InventoryList; TestInv->NextInventory != NULL; TestInv = TestInv->NextInventory)
			{
				if (TestInv->NextInventory == InvToRemove)
				{
					TestInv->NextInventory = InvToRemove->NextInventory;
					break;
				}
			}
		}
		if (InvToRemove == PendingWeapon)
		{
			PendingWeapon = NULL;
		}
		else if (InvToRemove == Weapon)
		{
			Weapon = NULL;
			if (PendingWeapon != NULL)
			{
				WeaponChanged();
			}
			else
			{
				WeaponClass = NULL;
				UpdateWeaponAttachment();
			}
			if (!bTearOff)
			{
				if (IsLocallyControlled())
				{
					SwitchToBestWeapon();
				}
				else
				{
					ClientWeaponLost((AUTWeapon*)InvToRemove);
				}
			}
		}
		InvToRemove->Removed();
	}
}

bool AUTCharacter::IsInInventory(const AUTInventory* TestInv) const
{
	for (AUTInventory* Inv = InventoryList; Inv != NULL; Inv = Inv->GetNext())
	{
		if (Inv == TestInv)
		{
			return true;
		}
	}

	return false;
}

void AUTCharacter::TossInventory(AUTInventory* InvToToss, FVector ExtraVelocity)
{
	if (InvToToss == NULL)
	{
		UE_LOG(UT, Warning, TEXT("TossInventory(): InvToToss == NULL"));
	}
	else if (!IsInInventory(InvToToss))
	{
		UE_LOG(UT, Warning, TEXT("Attempt to remove %s which is not in %s's inventory!"), *InvToToss->GetName(), *GetName());
	}
	else
	{
		InvToToss->DropFrom(GetActorLocation() + GetActorRotation().Vector() * GetSimpleCollisionCylinderExtent().X, GetVelocity() + GetActorRotation().RotateVector(ExtraVelocity + FVector(300.0f, 0.0f, 150.0f)));
	}
}

void AUTCharacter::DiscardAllInventory()
{
	AUTInventory* Inv = InventoryList;
	while (Inv != NULL)
	{
		AUTInventory* NextInv = Inv->GetNext();
		Inv->Destroy();
		Inv = NextInv;
	}
}

void AUTCharacter::InventoryEvent(FName EventName)
{
	for (AUTInventory* Inv = InventoryList; Inv != NULL; Inv = Inv->GetNext())
	{
		if (Inv->bCallOwnerEvent)
		{
			Inv->OwnerEvent(EventName);
		}
	}
}

void AUTCharacter::SwitchWeapon(AUTWeapon* NewWeapon)
{
	if (NewWeapon != NULL)
	{
		if (Role <= ROLE_SimulatedProxy)
		{
			UE_LOG(UT, Warning, TEXT("Illegal SwitchWeapon() call on remote client"));
		}
		else if (NewWeapon->Instigator != this || !IsInInventory(NewWeapon))
		{
			UE_LOG(UT, Warning, TEXT("Weapon %s is not owned by self"), *GetNameSafe(NewWeapon));
		}
		else if (Role == ROLE_Authority)
		{
			ClientSwitchWeapon(NewWeapon);
			// NOTE: we don't call LocalSwitchWeapon() here; we ping back from the client so all weapon switches are lead by the client
			//		otherwise, we get inconsistencies if both sides trigger weapon changes
		}
		else
		{
			LocalSwitchWeapon(NewWeapon);
			ServerSwitchWeapon(NewWeapon);
		}
	}
}

void AUTCharacter::LocalSwitchWeapon(AUTWeapon* NewWeapon)
{
	// make sure clients don't try to switch to weapons that haven't been fully replicated/initialized
	if (NewWeapon != NULL && NewWeapon->GetUTOwner() == NULL)
	{
		ClientSwitchWeapon(Weapon);
	}
	else
	{
		if (Weapon == NULL)
		{
			if (NewWeapon != NULL)
			{
				// initial equip
				PendingWeapon = NewWeapon;
				WeaponChanged();
			}
		}
		else if (NewWeapon != NULL)
		{
			if (NewWeapon != Weapon)
			{
				if (Weapon->PutDown())
				{
					// standard weapon switch to some other weapon
					PendingWeapon = NewWeapon;
				}
			}
			else if (PendingWeapon != NULL)
			{
				// switching back to weapon that was on its way down
				PendingWeapon = NULL;
				Weapon->BringUp();
			}
		}
		else if (Weapon != NULL && PendingWeapon != NULL && PendingWeapon->PutDown())
		{
			// stopping weapon switch in progress by passing NULL
			PendingWeapon = NULL;
			Weapon->BringUp();
		}
	}
}

void AUTCharacter::ClientSwitchWeapon_Implementation(AUTWeapon* NewWeapon)
{
	LocalSwitchWeapon(NewWeapon);
	if (Role < ROLE_Authority)
	{
		ServerSwitchWeapon(NewWeapon);
	}
}

void AUTCharacter::ServerSwitchWeapon_Implementation(AUTWeapon* NewWeapon)
{
	if (NewWeapon != NULL)
	{
		LocalSwitchWeapon(NewWeapon);
	}
}
bool AUTCharacter::ServerSwitchWeapon_Validate(AUTWeapon* NewWeapon)
{
	return true;
}

void AUTCharacter::WeaponChanged()
{
	if (PendingWeapon != NULL)
	{
		checkSlow(IsInInventory(PendingWeapon));
		Weapon = PendingWeapon;
		WeaponClass = Weapon->GetClass();
		UpdateWeaponAttachment();
		PendingWeapon = NULL;
		Weapon->BringUp();
	}
	else
	{
		// restore current weapon since pending became invalid
		Weapon->BringUp();
	}
}

void AUTCharacter::ClientWeaponLost_Implementation(AUTWeapon* LostWeapon)
{
	if (IsLocallyControlled() && Weapon == LostWeapon)
	{
		Weapon = NULL;
		if (!IsDead())
		{
			if (PendingWeapon == NULL)
			{
				SwitchToBestWeapon();
			}
			if (PendingWeapon != NULL)
			{
				WeaponChanged();
			}
		}
	}
}

void AUTCharacter::SwitchToBestWeapon()
{
	if (IsLocallyControlled())
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
		if (PC != NULL)
		{
			PC->SwitchToBestWeapon();
		}
		// TODO:  bots
	}
}

void AUTCharacter::UpdateWeaponAttachment()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		TSubclassOf<AUTWeaponAttachment> NewAttachmentClass = (WeaponClass != NULL) ? WeaponClass.GetDefaultObject()->AttachmentType : NULL;
		if (WeaponAttachment != NULL && (NewAttachmentClass == NULL || (WeaponAttachment != NULL && WeaponAttachment->GetClass() != NewAttachmentClass)))
		{
			WeaponAttachment->Destroy();
			WeaponAttachment = NULL;
		}
		if (WeaponAttachment == NULL && NewAttachmentClass != NULL)
		{
			FActorSpawnParameters Params;
			Params.Instigator = this;
			Params.Owner = this;
			WeaponAttachment = GetWorld()->SpawnActor<AUTWeaponAttachment>(NewAttachmentClass, Params);
		}
	}
}

float AUTCharacter::GetFireRateMultiplier()
{
	return FMath::Max<float>(FireRateMultiplier, 0.1f);
}
void AUTCharacter::SetFireRateMultiplier(float InMult)
{
	FireRateMultiplier = InMult;
	FireRateChanged();
}
void AUTCharacter::FireRateChanged()
{
	if (Weapon != NULL)
	{
		Weapon->UpdateTiming();
	}
}

void AUTCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, LastTakeHitInfo, GetWorld()->TimeSeconds - LastTakeHitTime < 1.0f);
}

void AUTCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTCharacter, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, InventoryList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashLocation, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, LastTakeHitInfo, COND_Custom);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponClass, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, DamageScaling, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireRateMultiplier, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, AmbientSound, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, CharOverlayFlags, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponOverlayFlags, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, ReplicatedBodyMaterial, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, HeadScale, COND_None);
}

void AUTCharacter::AddDefaultInventory(TArray<TSubclassOf<AUTInventory>> DefaultInventoryToAdd)
{
	// Add the default character inventory

	for (int i=0;i<DefaultCharacterInventory.Num();i++)
	{
		AddInventory(GetWorld()->SpawnActor<AUTInventory>(DefaultCharacterInventory[i], FVector(0.0f), FRotator(0, 0, 0)), true);
	}

	// Add the default inventory passed in from the game

	for (int i=0;i<DefaultInventoryToAdd.Num();i++)
	{
		AddInventory(GetWorld()->SpawnActor<AUTInventory>(DefaultInventoryToAdd[i], FVector(0.0f), FRotator(0, 0, 0)), true);
	}
}


// @TODO: These changes mimic changes to engine source that are pending (likely for 4.3), so remove this code once that is released.

namespace UTMovementBaseUtility
{
	void AddTickDependency(FTickFunction& BasedObjectTick, class UPrimitiveComponent* NewBase)
	{
		if (NewBase && MovementBaseUtility::UseRelativePosition(NewBase))
		{
			AActor* NewBaseOwner = NewBase->GetOwner();
			if (NewBaseOwner)
			{
				TArray<UActorComponent*> Components;
				NewBaseOwner->GetComponents(Components);
				for (auto& Component : Components)
				{
					BasedObjectTick.AddPrerequisite(Component, Component->PrimaryComponentTick);
				}
			}
		}
	}

	void RemoveTickDependency(FTickFunction& BasedObjectTick, class UPrimitiveComponent* OldBase)
	{
		if (OldBase && MovementBaseUtility::UseRelativePosition(OldBase))
		{
			AActor* OldBaseOwner = OldBase->GetOwner();
			if (OldBaseOwner)
			{
				TArray<UActorComponent*> Components;
				OldBaseOwner->GetComponents(Components);
				for (auto& Component : Components)
				{
					BasedObjectTick.RemovePrerequisite(Component, Component->PrimaryComponentTick);
				}
			}
		}
	}
}


void AUTCharacter::SetBase( UPrimitiveComponent* NewBaseComponent, bool bNotifyPawn )
{
	UPrimitiveComponent* OldBase = MovementBase;
	Super::SetBase(NewBaseComponent, bNotifyPawn);

	if (MovementBase != OldBase)
	{
		// Update tick order dependencies (tick after moving bases)
		UTMovementBaseUtility::RemoveTickDependency(CharacterMovement->PrimaryComponentTick, OldBase);
		UTMovementBaseUtility::AddTickDependency(CharacterMovement->PrimaryComponentTick, MovementBase);
	}
}

bool AUTCharacter::CanDodge() const
{
	return !bIsCrouched && Cast<UUTCharacterMovement>(CharacterMovement) && Cast<UUTCharacterMovement>(CharacterMovement)->CanDodge(); 
}

bool AUTCharacter::Dodge(FVector DodgeDir, FVector DodgeCross)
{
	if (CanDodge())
	{
		if ( DodgeOverride(DodgeDir, DodgeCross) )
		{
			// blueprint handled dodge attempt
			return true;
		}
		if (Cast<UUTCharacterMovement>(CharacterMovement) && Cast<UUTCharacterMovement>(CharacterMovement)->PerformDodge(DodgeDir, DodgeCross))
		{
			OnDodge(DodgeDir);
			// @TODO FIXMESTEVE need to cause falling damage based on pre-dodge falling Velocity.Z if was falling
			return true;
		}
	}
	return false;
}

bool AUTCharacter::CanJumpInternal_Implementation() const
{
	// @TODO FIXMESTEVE ask to get this mostly moved to CharacterMovement!
	return !bIsCrouched && Cast<UUTCharacterMovement>(CharacterMovement) && (CharacterMovement->IsMovingOnGround() || Cast<UUTCharacterMovement>(CharacterMovement)->CanMultiJump()) && CharacterMovement->CanEverJump() && !CharacterMovement->bWantsToCrouch;
}

void AUTCharacter::CheckJumpInput(float DeltaTime)
{
	if (bPressedJump)
	{
		DoJump(bClientUpdating);
	}
	else
	{
		UUTCharacterMovement* MyMovement = Cast<UUTCharacterMovement>(CharacterMovement);
		if (MyMovement->bPressedDodgeForward || MyMovement->bPressedDodgeBack || MyMovement->bPressedDodgeLeft || MyMovement->bPressedDodgeRight)
		{
			float DodgeDirX = MyMovement->bPressedDodgeForward ? 1.f : (MyMovement->bPressedDodgeBack ? -1.f : 0.f);
			float DodgeDirY = MyMovement->bPressedDodgeLeft ? -1.f : (MyMovement->bPressedDodgeRight ? 1.f : 0.f);
			float DodgeCrossX = (MyMovement->bPressedDodgeLeft || MyMovement->bPressedDodgeRight) ? 1.f : 0.f;
			float DodgeCrossY = (MyMovement->bPressedDodgeForward || MyMovement->bPressedDodgeBack) ? 1.f : 0.f;
			FRotator TurnRot(0.f, GetActorRotation().Yaw, 0.f);
			FRotationMatrix TurnRotMatrix = FRotationMatrix(TurnRot);
			FVector X = TurnRotMatrix.GetScaledAxis(EAxis::X);
			FVector Y = TurnRotMatrix.GetScaledAxis(EAxis::Y);
			Dodge(DodgeDirX*X + DodgeDirY*Y, DodgeCrossX*X + DodgeCrossY*Y);
		}
	}
}

void AUTCharacter::ClearJumpInput()
{
	Super::ClearJumpInput();
	UUTCharacterMovement* UTCharMov = Cast<UUTCharacterMovement>(CharacterMovement);
	if (UTCharMov)
	{
		UTCharMov->ClearJumpInput();
	}
}

void AUTCharacter::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	UUTCharacterMovement* UTCharMov = Cast<UUTCharacterMovement>(CharacterMovement);
	if (UTCharMov)
	{
		int32 DodgeFlags = (Flags >> 2) & 7;
		UTCharMov->bPressedDodgeForward = (DodgeFlags == 1);
		UTCharMov->bPressedDodgeBack = (DodgeFlags == 2);
		UTCharMov->bPressedDodgeLeft = (DodgeFlags == 3);
		UTCharMov->bPressedDodgeRight = (DodgeFlags == 4);
		UTCharMov->bIsSprinting = (DodgeFlags == 5);
	}
}

APlayerCameraManager* AUTCharacter::GetPlayerCameraManager()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
	return PC != NULL ? PC->PlayerCameraManager : NULL;
}

void AUTCharacter::PlayFootstep(uint8 FootNum)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), FootstepSound, this, SRT_IfSourceNotReplicated);
	LastFoot = FootNum;
	LastFootstepTime = GetWorld()->TimeSeconds;
}

FVector AUTCharacter::GetPawnViewLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight) + EyeOffset;
}

void AUTCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bFindCameraComponentWhenViewTarget && CharacterCameraComponent && CharacterCameraComponent->bIsActive)
	{
		CharacterCameraComponent->GetCameraView(DeltaTime, OutResult);
		OutResult.Location += EyeOffset;
		return;
	}

	GetActorEyesViewPoint(OutResult.Location, OutResult.Rotation);
}

void AUTCharacter::PlayJump()
{
	DesiredJumpBob = WeaponJumpBob;
	UUTGameplayStatics::UTPlaySound(GetWorld(), JumpSound, this, SRT_AllButOwner);
}

void AUTCharacter::OnDodge_Implementation(const FVector &DodgeDir)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), DodgeSound, this, SRT_AllButOwner);
}

void AUTCharacter::Landed(const FHitResult& Hit)
{
	// cause crushing damage if we fell on another character
	if (Cast<AUTCharacter>(Hit.Actor.Get()) != NULL)
	{
		float Damage = CrushingDamageFactor * CharacterMovement->Velocity.Z / -100.0f;
		if (Damage >= 1.0f)
		{
			FUTPointDamageEvent DamageEvent(Damage, Hit, -CharacterMovement->Velocity.SafeNormal(), UUTDamageType::StaticClass());
			Hit.Actor->TakeDamage(Damage, DamageEvent, Controller, this);
		}
	}

	if (Role == ROLE_Authority)
	{
		MakeNoise(FMath::Clamp<float>(CharacterMovement->Velocity.Z / (MaxSafeFallSpeed * -0.5f), 0.0f, 1.0f));
	}

	// bob weapon and viewpoint on landing
	if (CharacterMovement->Velocity.Z < -200.f)
	{
		DesiredJumpBob = WeaponLandBob* FMath::Min(1.f, (-1.f*CharacterMovement->Velocity.Z - 100.f) / 700.f);
		if (CharacterMovement->Velocity.Z < -300.f)
		{
			TargetEyeOffset.Z = EyeOffsetLandBob * FMath::Min(1.f, (-1.f*CharacterMovement->Velocity.Z - 245.f) / 700.f);
		}
	}

	TakeFallingDamage(Hit);

	Super::Landed(Hit);

	static FName NAME_Landed(TEXT("Landed"));
	InventoryEvent(NAME_Landed);

	LastHitBy = NULL;

	UUTGameplayStatics::UTPlaySound(GetWorld(), LandingSound, this, SRT_None);
}

void AUTCharacter::TakeFallingDamage(const FHitResult& Hit)
{
	if (Role == ROLE_Authority)
	{
		float FallingSpeed = CharacterMovement->Velocity.Z;
		if (FallingSpeed < -1.f * MaxSafeFallSpeed)
		{
			/* TODO: water
			if (IsTouchingWaterVolume())
			{
				FallingSpeed += 100.f;
			}*/
			if (FallingSpeed < -1.f * MaxSafeFallSpeed)
			{
				FUTPointDamageEvent DamageEvent(-100.f * (FallingSpeed + MaxSafeFallSpeed) / MaxSafeFallSpeed, Hit, CharacterMovement->Velocity.SafeNormal(), UUTDmgType_Fell::StaticClass());
				if (DamageEvent.Damage >= 1.0f)
				{
					TakeDamage(DamageEvent.Damage, DamageEvent, Controller, this);
				}
			}
		}
	}
}

void AUTCharacter::SetCharacterOverlay(UMaterialInterface* NewOverlay, bool bEnabled)
{
	if (Role == ROLE_Authority && NewOverlay != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			int32 Index = GS->FindOverlayMaterial(NewOverlay);
			if (Index == INDEX_NONE)
			{
				UE_LOG(UT, Warning, TEXT("Overlay material %s was not registered"), *NewOverlay->GetFullName());
			}
			else
			{
				checkSlow(Index < sizeof(CharOverlayFlags * 8));
				if (bEnabled)
				{
					CharOverlayFlags |= (1 << Index);
				}
				else
				{
					CharOverlayFlags &= ~(1 << Index);
				}
				if (GetNetMode() != NM_DedicatedServer)
				{
					UpdateCharOverlays();
				}
			}
		}
	}
}
void AUTCharacter::SetWeaponOverlay(UMaterialInterface* NewOverlay, bool bEnabled)
{
	if (Role == ROLE_Authority && NewOverlay != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			int32 Index = GS->FindOverlayMaterial(NewOverlay);
			if (Index == INDEX_NONE)
			{
				UE_LOG(UT, Warning, TEXT("Overlay material %s was not registered"), *NewOverlay->GetFullName());
			}
			else
			{
				checkSlow(Index < sizeof(WeaponOverlayFlags * 8));
				if (bEnabled)
				{
					WeaponOverlayFlags |= (1 << Index);
				}
				else
				{
					WeaponOverlayFlags &= ~(1 << Index);
				}
				if (GetNetMode() != NM_DedicatedServer)
				{
					UpdateWeaponOverlays();
				}
			}
		}
	}
}

void AUTCharacter::UpdateCharOverlays()
{
	if (CharOverlayFlags == 0)
	{
		if (OverlayMesh != NULL && OverlayMesh->IsRegistered())
		{
			OverlayMesh->DetachFromParent();
			OverlayMesh->UnregisterComponent();
		}
	}
	else
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			if (OverlayMesh == NULL)
			{
				OverlayMesh = ConstructObject<USkeletalMeshComponent>(Mesh->GetClass(), this, NAME_None, RF_NoFlags, Mesh, true);
				OverlayMesh->AttachParent = NULL; // this gets copied but we don't want it to be
				OverlayMesh->SetMasterPoseComponent(Mesh);
			}
			if (!OverlayMesh->IsRegistered())
			{
				OverlayMesh->RegisterComponent();
				OverlayMesh->AttachTo(Mesh, NAME_None, EAttachLocation::SnapToTarget);
				OverlayMesh->SetRelativeScale3D(FVector(1.015f, 1.015f, 1.015f));
			}
			UMaterialInterface* FirstOverlay = GS->GetFirstOverlay(CharOverlayFlags);
			for (int32 i = 0; i < OverlayMesh->GetNumMaterials(); i++)
			{
				OverlayMesh->SetMaterial(i, FirstOverlay);
			}
		}
	}
}
void AUTCharacter::UpdateWeaponOverlays()
{
	if (Weapon != NULL)
	{
		Weapon->UpdateOverlays();
	}
	if (WeaponAttachment != NULL)
	{
		WeaponAttachment->UpdateOverlays();
	}
}

void AUTCharacter::SetSkin(UMaterialInterface* NewSkin)
{
	ReplicatedBodyMaterial = NewSkin;
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateSkin();
	}
}
void AUTCharacter::UpdateSkin()
{
	if (ReplicatedBodyMaterial != NULL)
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, ReplicatedBodyMaterial);
		}
	}
	else
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, GetClass()->GetDefaultObject<AUTCharacter>()->Mesh->GetMaterial(i));
		}
	}
	if (Weapon != NULL)
	{
		Weapon->SetSkin(ReplicatedBodyMaterial);
	}
	if (WeaponAttachment != NULL)
	{
		WeaponAttachment->SetSkin(ReplicatedBodyMaterial);
	}
}

void AUTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Mesh->MeshComponentUpdateFlag >= EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered && !Mesh->bRecentlyRendered && !IsLocallyControlled())
	{
		// TODO: currently using an arbitrary made up interval and scale factor
		// TODO: for local player in first person, sync to view bob, etc
		float Speed = CharacterMovement->Velocity.Size();
		if (CharacterMovement->MovementMode == MOVE_Walking && Speed > 0.0f && GetWorld()->TimeSeconds - LastFootstepTime > 0.35f * CharacterMovement->MaxWalkSpeed / Speed)
		{
			PlayFootstep((LastFoot + 1) & 1);
		}
	}

	// update eyeheight locally
	float InterpTime = FMath::Min(1.f, EyeOffsetInterpRate*DeltaTime);
	EyeOffset = (1.f - InterpTime)*EyeOffset + InterpTime*TargetEyeOffset;
	TargetEyeOffset *= FMath::Max(0.f, 1.f - EyeOffsetDecayRate*DeltaTime);
	/*
	if (CharacterMovement && ((CharacterMovement->GetCurrentAcceleration() | CharacterMovement->Velocity) < 0.f))
	{
		UE_LOG(UT, Warning, TEXT("Position %f %f time %f"),GetActorLocation().X, GetActorLocation().Y, GetWorld()->GetTimeSeconds());
	}*/
}

void AUTCharacter::BecomeViewTarget(class APlayerController* PC)
{
	Super::BecomeViewTarget(PC);

	if (PC != NULL)
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
		if (UTPC != NULL && UTPC->IsLocalController() && UTPC->GetUTCharacter() == this)
		{
			SetMeshVisibility(UTPC->IsBehindView());
			return;
		}
	}

	SetMeshVisibility(true);
}

void AUTCharacter::EndViewTarget( class APlayerController* PC )
{
	Super::EndViewTarget(PC);
	if (PC != NULL && PC->IsLocalController())
	{
		SetMeshVisibility(true);
	}
}

uint8 AUTCharacter::GetTeamNum() const
{
	const IUTTeamInterface* TeamInterface = InterfaceCast<IUTTeamInterface>(Controller);
	if (TeamInterface != NULL)
	{
		return TeamInterface->GetTeamNum();
	}
	else
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		return (PS != NULL && PS->Team != NULL) ? PS->Team->TeamIndex : 255;
	}
}

void AUTCharacter::PossessedBy(AController* NewController)
{
	// TODO: shouldn't base class do this? APawn::Unpossessed() still does SetOwner(NULL)...
	SetOwner(NewController);

	Super::PossessedBy(NewController);
	NotifyTeamChanged();
}

void AUTCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (PlayerState != NULL)
	{
		NotifyTeamChanged();
	}
}

void AUTCharacter::NotifyTeamChanged()
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL && PS->Team != NULL && BodyMI != NULL)
	{
		static FName NAME_TeamColor(TEXT("TeamColor"));
		BodyMI->SetVectorParameterValue(NAME_TeamColor, PS->Team->TeamColor);
	}
}

void AUTCharacter::PlayerChangedTeam()
{
	PlayerSuicide();
}

void AUTCharacter::PlayerSuicide()
{
	if (Role == ROLE_Authority)
	{
		FHitResult FakeHit(this, NULL, GetActorLocation(), GetActorRotation().Vector());
		FUTPointDamageEvent FakeDamageEvent(0, FakeHit, FVector(0, 0, 0), UUTDmgType_Suicide::StaticClass());
		Died(NULL, FakeDamageEvent);
	}
}
