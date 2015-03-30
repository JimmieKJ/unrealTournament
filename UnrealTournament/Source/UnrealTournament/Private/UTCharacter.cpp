// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTCharacterMovement.h"
#include "UTWeaponAttachment.h"
#include "UnrealNetwork.h"
#include "UTDmgType_Suicide.h"
#include "UTDmgType_Fell.h"
#include "UTDmgType_Drown.h"
#include "UTDmgType_FallingCrush.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTTeamGameMode.h"
#include "UTDmgType_Telefragged.h"
#include "UTDmgType_BlockedTelefrag.h"
#include "UTDmgType_FeignFail.h"
#include "UTReplicatedEmitter.h"
#include "UTWorldSettings.h"
#include "UTArmor.h"
#include "UTImpactEffect.h"
#include "UTGib.h"
#include "UTRemoteRedeemer.h"
#include "UTDroppedPickup.h"
#include "UTWeaponStateFiring.h"
#include "UTMovementBaseInterface.h"
#include "UTCharacterContent.h"
#include "ComponentReregisterContext.h"

UUTMovementBaseInterface::UUTMovementBaseInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

//////////////////////////////////////////////////////////////////////////
// AUTCharacter

DEFINE_LOG_CATEGORY_STATIC(LogUTCharacter, Log, All);

AUTCharacter::AUTCharacter(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	// FIXME: TEMP FOR GDC: needed because of TeamMaterials
	static ConstructorHelpers::FObjectFinder<UClass> DefaultCharContent(TEXT("Class'/Game/RestrictedAssets/Character/Malcom_New/Malcolm_New.Malcolm_New_C'"));

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(46.f, 92.0f);

	// Create a CameraComponent	
	CharacterCameraComponent = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("FirstPersonCamera"));
	CharacterCameraComponent->AttachParent = GetCapsuleComponent();
	DefaultBaseEyeHeight = 71.f;
	BaseEyeHeight = DefaultBaseEyeHeight;
	CharacterCameraComponent->RelativeLocation = FVector(0, 0, DefaultBaseEyeHeight); // Position the camera

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	FirstPersonMesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("CharacterMesh1P"));
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->AttachParent = CharacterCameraComponent;
	FirstPersonMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;

	GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->bEnablePhysicsOnDedicatedServer = true; // needed for feign death; death ragdoll shouldn't be invoked on server
	GetMesh()->bReceivesDecals = false;
	GetMesh()->bLightAttachmentsAsGroup = true;

	UTCharacterMovement = Cast<UUTCharacterMovement>(GetCharacterMovement());

	HealthMax = 100;
	SuperHealthMax = 199;
	DamageScaling = 1.0f;
	FireRateMultiplier = 1.0f;
	bSpawnProtectionEligible = true;
	MaxSafeFallSpeed = 2400.0f;
	FallingDamageFactor = 100.0f;
	CrushingDamageFactor = 2.0f;
	HeadScale = 1.0f;
	HeadRadius = 18.0f;
	HeadHeight = 8.0f;
	HeadBone = FName(TEXT("head"));
	EmoteSpeed = 1.0f;
	UnfeignCount = 0;

	BobTime = 0.f;
	WeaponBobMagnitude = FVector(0.f, 0.8f, 0.4f);
	WeaponJumpBob = FVector(0.f, 0.f, -3.6f);
	WeaponDodgeBob = FVector(0.f, 6.f, -2.5f);
	WeaponLandBob = FVector(0.f, 0.f, 10.5f);
	WeaponSlideBob = FVector(0.f, 12.f, 15.f);
	WeaponBreathingBobRate = 0.2f;
	WeaponRunningBobRate = 0.8f;
	WeaponJumpBobInterpRate = 6.5f;
	WeaponHorizontalBobInterpRate = 4.3f;
	WeaponLandBobDecayRate = 5.f;
	EyeOffset = FVector(0.f, 0.f, 0.f);
	CrouchEyeOffset = EyeOffset;
	TargetEyeOffset = EyeOffset;
	EyeOffsetInterpRate = FVector(18.f, 9.f, 9.f);
	CrouchEyeOffsetInterpRate = 6.f;
	EyeOffsetDecayRate = FVector(7.f, 7.f, 7.f);
	EyeOffsetJumpBob = 20.f;
	EyeOffsetLandBob = -110.f;
	EyeOffsetLandBobThreshold = 300.f;
	WeaponLandBobThreshold = 100.f;
	FullWeaponLandBobVelZ = 900.f;
	FullEyeOffsetLandBobVelZ = 750.f;
	WeaponDirChangeDeflection = 4.f;
	RagdollBlendOutTime = 0.75f;
	bApplyWallSlide = false;
	FeignNudgeMag = 100000.f;

	MinPainSoundInterval = 0.35f;
	LastPainSoundTime = -100.0f;
	bCanPlayWallHitSound = true;

	SprintAmbientStartSpeed = 1000.f;
	FallingAmbientStartSpeed = -1300.f;

	PrimaryActorTick.bStartWithTickEnabled = true;

	// TODO: write real relevancy checking
	NetCullDistanceSquared = 500000000.0f;

	OnActorBeginOverlap.AddDynamic(this, &AUTCharacter::OnOverlapBegin);
	GetMesh()->OnComponentHit.AddDynamic(this, &AUTCharacter::OnRagdollCollision);
	GetMesh()->SetNotifyRigidBodyCollision(true);

	TeamPlayerIndicatorMaxDistance = 2700.0f;
	PlayerIndicatorMaxDistance = 1200.f;
	MaxSavedPositionAge = 0.3f; // @TODO FIXMESTEVE should use server's MaxPredictionPing to determine this - note also that bots will increase this if needed to satisfy their tracking requirements
	MaxShotSynchDelay = 0.1f;

	MaxStackedArmor = 200;
	MaxDeathLifeSpan = 30.0f;
	MinWaterSoundInterval = 0.8f;
	LastWaterSoundTime = 0.f;
	DrowningDamagePerSecond = 2.f;
	MaxUnderWaterTime = 30.f;
	bHeadIsUnderwater = false;
	LastBreathTime = 0.f;
	LastDrownTime = 0.f;

	LowHealthAmbientThreshold = 40;
	MinOverlapToTelefrag = 25.f;
}

void AUTCharacter::SetBase(UPrimitiveComponent* NewBaseComponent, const FName BoneName, bool bNotifyPawn)
{
	// @TODO FIXMESTEVE - BaseChange() would be useful for this if it passed the old base as well
	AActor* OldMovementBase = GetMovementBaseActor(this);
	Super::SetBase(NewBaseComponent, BoneName, bNotifyPawn);
	if (GetCharacterMovement() && GetCharacterMovement()->MovementMode != MOVE_None)
	{
		AActor* NewMovementBase = GetMovementBaseActor(this);
		if (NewMovementBase != OldMovementBase)
		{
			if (OldMovementBase && OldMovementBase->GetClass()->ImplementsInterface(UUTMovementBaseInterface::StaticClass()))
			{
				IUTMovementBaseInterface::Execute_RemoveBasedCharacter(OldMovementBase, this);
			}

			if (NewMovementBase && NewMovementBase->GetClass()->ImplementsInterface(UUTMovementBaseInterface::StaticClass()))
			{
				IUTMovementBaseInterface::Execute_AddBasedCharacter(NewMovementBase, this);
			}
		}
	}
}

void AUTCharacter::PlayWaterSound(USoundBase* WaterSound)
{
	if (WaterSound && (GetWorld()->GetTimeSeconds() - LastWaterSoundTime > MinWaterSoundInterval))
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), WaterSound, this, SRT_None);
		LastWaterSoundTime = GetWorld()->GetTimeSeconds();
	}
}


void AUTCharacter::OnWalkingOffLedge_Implementation()
{
	AUTBot* B = Cast<AUTBot>(Controller);
	if (B != NULL)
	{
		B->NotifyWalkingOffLedge();
	}
}

void AUTCharacter::BeginPlay()
{
	GetMesh()->SetOwnerNoSee(false); // compatibility with old content, we're doing this through UpdateHiddenComponents() now

	if (GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->AddPostRenderedActor(this);
		}
	}
	if (Health == 0 && Role == ROLE_Authority)
	{
		Health = HealthMax;
	}
	CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, DefaultBaseEyeHeight), false);
	if (CharacterCameraComponent->RelativeLocation.Size2D() > 0.0f)
	{
		UE_LOG(UT, Warning, TEXT("%s: CameraComponent shouldn't have X/Y translation!"), *GetName());
	}
	Super::BeginPlay();
}

void AUTCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (GetNetMode() != NM_DedicatedServer)
	{
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); i++)
		{
			// FIXME: NULL check is hack for editor reimport bug breaking number of materials
			if (GetMesh()->GetMaterial(i) != NULL)
			{
				BodyMIs.Add(GetMesh()->CreateAndSetMaterialInstanceDynamic(i));
			}
		}
	}
}

void AUTCharacter::NotifyPendingServerFire()
{
	if (SavedPositions.Num() > 0)
	{
		SavedPositions.Last().bShotSpawned = true;
	}
}

bool AUTCharacter::DelayedShotFound()
{
	const float WorldTime = GetWorld()->GetTimeSeconds();
	for (int32 i = SavedPositions.Num() - 1; i >= 0; i--)
	{
		if (SavedPositions[i].bShotSpawned)
		{
			return true;
		}
		if (WorldTime - SavedPositions[i].Time > MaxShotSynchDelay)
		{
			break;
		}
	}
	return false;
}

FVector AUTCharacter::GetDelayedShotPosition()
{
	const float WorldTime = GetWorld()->GetTimeSeconds();
	for (int32 i = SavedPositions.Num() - 1; i >= 0; i--)
	{
		if (SavedPositions[i].bShotSpawned)
		{
			return SavedPositions[i].Position;
		}
		if (WorldTime - SavedPositions[i].Time > MaxShotSynchDelay)
		{
			break;
		}
	}
	return GetActorLocation();
}

FRotator AUTCharacter::GetDelayedShotRotation()
{
	const float WorldTime = GetWorld()->GetTimeSeconds();
	for (int32 i = SavedPositions.Num() - 1; i >= 0; i--)
	{
		if (SavedPositions[i].bShotSpawned)
		{
			return SavedPositions[i].Rotation;
		}
		if (WorldTime - SavedPositions[i].Time > MaxShotSynchDelay)
		{
			break;
		}
	}
	return GetViewRotation();
}

void AUTCharacter::PositionUpdated(bool bShotSpawned)
{
	const float WorldTime = GetWorld()->GetTimeSeconds();
	new(SavedPositions) FSavedPosition(GetActorLocation(), GetViewRotation(), GetCharacterMovement()->Velocity, GetCharacterMovement()->bJustTeleported, bShotSpawned, WorldTime, (UTCharacterMovement ? UTCharacterMovement->GetCurrentSynchTime() : 0.f));

	// maintain one position beyond MaxSavedPositionAge for interpolation
	if (SavedPositions.Num() > 1 && SavedPositions[1].Time < WorldTime - MaxSavedPositionAge)
	{
		SavedPositions.RemoveAt(0);
	}
}

FVector AUTCharacter::GetRewindLocation(float PredictionTime)
{
	FVector TargetLocation = GetActorLocation();
	float TargetTime = GetWorld()->GetTimeSeconds() - PredictionTime;
	if (PredictionTime > 0.f)
	{
		for (int32 i=SavedPositions.Num()-1; i >= 0; i--)
		{
			TargetLocation = SavedPositions[i].Position;
			if (SavedPositions[i].Time < TargetTime)
			{
				if (!SavedPositions[i].bTeleported && (i<SavedPositions.Num()-1))
				{
					float Percent = (SavedPositions[i + 1].Time == SavedPositions[i].Time) ? 1.f : (TargetTime - SavedPositions[i].Time) / (SavedPositions[i + 1].Time - SavedPositions[i].Time);
					TargetLocation = SavedPositions[i].Position + Percent * (SavedPositions[i + 1].Position - SavedPositions[i].Position);
				}
				break;
			}
		}
	}
	return TargetLocation;
}

void AUTCharacter::GetSimplifiedSavedPositions(TArray<FSavedPosition>& OutPositions) const
{
	OutPositions.Empty(SavedPositions.Num());
	if (SavedPositions.Num() > 0)
	{
		OutPositions.Add(SavedPositions[0]);
		for (int32 i = 1; i < SavedPositions.Num(); i++)
		{
			if (OutPositions.Last().Time < SavedPositions[i].Time)
			{
				OutPositions.Add(SavedPositions[i]);
			}
		}
	}
}

void AUTCharacter::RecalculateBaseEyeHeight()
{
	BaseEyeHeight = (bIsCrouched || (UTCharacterMovement && UTCharacterMovement->bIsDodgeRolling)) ? CrouchedEyeHeight : DefaultBaseEyeHeight;
}

void AUTCharacter::Crouch(bool bClientSimulation)
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bWantsToCrouch = true;
	}
}

void AUTCharacter::OnEndCrouch(float HeightAdjust, float ScaledHeightAdjust)
{
	if (UTCharacterMovement->bPressedSlide)
	{
		Roll(GetVelocity().GetSafeNormal());
		UTCharacterMovement->bPressedSlide = false;
	}
	else
	{
		Super::OnEndCrouch(HeightAdjust, ScaledHeightAdjust);
		CrouchEyeOffset.Z += CrouchedEyeHeight - DefaultBaseEyeHeight - HeightAdjust;
		UTCharacterMovement->OldZ = GetActorLocation().Z;
		CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, DefaultBaseEyeHeight), false);
	}
}

void AUTCharacter::OnStartCrouch(float HeightAdjust, float ScaledHeightAdjust)
{
	Super::OnStartCrouch(HeightAdjust, ScaledHeightAdjust);
	CrouchEyeOffset.Z += DefaultBaseEyeHeight - CrouchedEyeHeight + HeightAdjust;
	UTCharacterMovement->OldZ = GetActorLocation().Z;
	CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, CrouchedEyeHeight),false);

	// Kill any montages that might be overriding the crouch anim
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance != NULL)
	{
		AnimInstance->Montage_Stop(0.2f);
	}
}

void AUTCharacter::Restart()
{
	Super::Restart();
	ClearJumpInput();

	// make sure equipped weapon state is synchronized
	if (IsLocallyControlled())
	{
		if (PendingWeapon != NULL)
		{
			SwitchWeapon(PendingWeapon);
		}
		else if (Weapon != NULL && Weapon->HasAnyAmmo())
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

bool AUTCharacter::IsSpawnProtected()
{
	if (!bSpawnProtectionEligible)
	{
		return false;
	}
	else
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		return (GS != NULL && GS->SpawnProtectionTime > 0.0f && GetWorld()->TimeSeconds - CreationTime < GS->SpawnProtectionTime);
	}
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

static TAutoConsoleVariable<int32> CVarDebugHeadshots(
	TEXT("p.DebugHeadshots"),
	0,
	TEXT("Debug headshot traces"),
	ECVF_Default);

FVector AUTCharacter::GetHeadLocation(float PredictionTime)
{
	// force mesh update if necessary
	if (!GetMesh()->ShouldTickPose())
	{
		GetMesh()->TickAnimation(0.0f);
		GetMesh()->RefreshBoneTransforms();
		GetMesh()->UpdateComponentToWorld();
	}
	FVector Result = GetMesh()->GetSocketLocation(HeadBone) + FVector(0.0f, 0.0f, HeadHeight);
	
	// offset based on PredictionTime to previous position
	return Result + GetRewindLocation(PredictionTime) - GetActorLocation();
}

bool AUTCharacter::IsHeadShot(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, bool bConsumeArmor, AUTCharacter* ShotInstigator, float PredictionTime)
{
	if (UTCharacterMovement && UTCharacterMovement->bIsDodgeRolling)
	{
		// no headshots while dodge rolling
		return false;
	}

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (ShotInstigator && GS && GS->OnSameTeam(this, ShotInstigator))
	{
		// teammates don't register headshots on each other
		return false;
	}

	FVector HeadLocation = GetHeadLocation();
	bool bHeadShot = FMath::PointDistToLine(HeadLocation, ShotDirection, HitLocation) < HeadRadius * HeadScale * WeaponHeadScaling;

	if (CVarDebugHeadshots.GetValueOnGameThread() != 0)
	{
		DrawDebugLine(GetWorld(), HitLocation + (ShotDirection * 1000.f), HitLocation - (ShotDirection * 1000.f), FColor::White, true);
		if (bHeadShot)
		{
			DrawDebugSphere(GetWorld(), HeadLocation, HeadRadius * HeadScale * WeaponHeadScaling, 10, FColor::Green, true);
		}
		else
		{
			DrawDebugSphere(GetWorld(), HeadLocation, HeadRadius * HeadScale * WeaponHeadScaling, 10, FColor::Red, true);
		}
	}

	if (bHeadShot)
	{
		// check for inventory items that prevent headshots
		for (TInventoryIterator<> It(this); It; ++It)
		{
			if (It->bCallDamageEvents && It->PreventHeadShot(HitLocation, ShotDirection, WeaponHeadScaling, bConsumeArmor))
			{
				HeadArmorFlashCount++;
				LastHeadArmorFlashTime = GetWorld()->GetTimeSeconds();
				if (GetNetMode() == NM_Standalone)
				{
					OnRepHeadArmorFlashCount();
				}
				if (ShotInstigator)
				{
					ShotInstigator->HeadShotBlocked();
				}

				// @TODO FIXMESTEVE - hack - need more elegant way of keeping headshots w/ udamage
				if (!ShotInstigator || (ShotInstigator->DamageScaling < 2.f))
				{
					bHeadShot = false;
				}
				break;
			}
		}
	}
	return bHeadShot;
}

void AUTCharacter::OnRepHeadArmorFlashCount()
{
	// play helmet client-side hit effect 
	if (HeadArmorHitEffect != NULL)
	{
		UParticleSystemComponent* PSC = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass(), this);
		PSC->bAutoDestroy = true;
		PSC->SecondsBeforeInactive = 0.0f;
		PSC->bAutoActivate = false;
		PSC->SetTemplate(HeadArmorHitEffect);
		PSC->bOverrideLODMethod = false;
		PSC->RegisterComponent();
		PSC->AttachTo(GetMesh(), HeadBone);
		PSC->ActivateSystem(true);
	}
}

void AUTCharacter::HeadShotBlocked()
{
	// locally play on instigator a sound to clearly signify headshot block
	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetController());
	if (MyPC)
	{
		MyPC->ClientPlaySound(HeadShotBlockedSound, 10.f);
	}
}

FVector AUTCharacter::GetWeaponBobOffset(float DeltaTime, AUTWeapon* MyWeapon)
{
	FRotationMatrix RotMatrix = FRotationMatrix(GetViewRotation());
	FVector X = RotMatrix.GetScaledAxis(EAxis::X);
	FVector Y = RotMatrix.GetScaledAxis(EAxis::Y);
	FVector Z = RotMatrix.GetScaledAxis(EAxis::Z);

	float InterpTime = FMath::Min(1.f, WeaponJumpBobInterpRate*DeltaTime);
	if (!GetCharacterMovement() || GetCharacterMovement()->IsFalling() || !MyWeapon)
	{
		// interp out weapon bob if falling
		BobTime = 0.f;
		CurrentWeaponBob.Y *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
		CurrentWeaponBob.Z *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
	}
	else
	{
		float Speed = GetCharacterMovement()->Velocity.Size();
		float LastBobTime = BobTime;
		float BobFactor = (WeaponBreathingBobRate + WeaponRunningBobRate*Speed / GetCharacterMovement()->MaxWalkSpeed);
		BobTime += DeltaTime * BobFactor;
		DesiredJumpBob *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
		FVector AccelDir = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal();
		if ((AccelDir | GetCharacterMovement()->Velocity) < 0.5f*GetCharacterMovement()->MaxWalkSpeed)
		{
			if ((AccelDir | Y) > 0.65f)
			{
				DesiredJumpBob.Y = -1.f*WeaponDirChangeDeflection;
			}
			else if ((AccelDir | Y) < -0.65f)
			{
				DesiredJumpBob.Y = WeaponDirChangeDeflection;
			}
		}
		CurrentWeaponBob.X = 0.f;
		if (UTCharacterMovement && UTCharacterMovement->bIsDodgeRolling)
		{
			// interp out weapon bob when dodge rolling and bring weapon up and in
			BobTime = 0.f;
			CurrentWeaponBob.Y *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
			CurrentWeaponBob.Z *= FMath::Max(0.f, 1.f - WeaponLandBobDecayRate*DeltaTime);
		}
		else
		{
			CurrentWeaponBob.Y = WeaponBobMagnitude.Y*BobFactor * FMath::Sin(8.f*BobTime);
			CurrentWeaponBob.Z = WeaponBobMagnitude.Z*BobFactor * FMath::Sin(16.f*BobTime);
		}

		// play footstep sounds when weapon changes bob direction if walking
		if (GetCharacterMovement()->MovementMode == MOVE_Walking && Speed > 10.0f && !bIsCrouched && (FMath::FloorToInt(0.5f + 8.f*BobTime / PI) != FMath::FloorToInt(0.5f + 8.f*LastBobTime / PI))
			&& (GetMesh()->MeshComponentUpdateFlag >= EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered) && !GetMesh()->bRecentlyRendered)
		{
			PlayFootstep((LastFoot + 1) & 1);
		}
	}
	float JumpYInterp = ((DesiredJumpBob.Y == 0.f) || (DesiredJumpBob.Z == 0.f)) ? FMath::Min(1.f, WeaponJumpBobInterpRate*DeltaTime) : FMath::Min(1.f, WeaponHorizontalBobInterpRate*FMath::Abs(DesiredJumpBob.Y)*DeltaTime);
	CurrentJumpBob.X = (1.f - InterpTime)*CurrentJumpBob.X + InterpTime*DesiredJumpBob.X;
	CurrentJumpBob.Y = (1.f - JumpYInterp)*CurrentJumpBob.Y + JumpYInterp*DesiredJumpBob.Y;
	CurrentJumpBob.Z = (1.f - InterpTime)*CurrentJumpBob.Z + InterpTime*DesiredJumpBob.Z;

	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetController()); // fixmesteve use the viewer rather than the controller
	float WeaponBobGlobalScaling = MyWeapon->WeaponBobScaling * (MyPC ? MyPC->WeaponBobGlobalScaling : 1.f);
	float EyeOffsetGlobalScaling = MyPC ? MyPC->EyeOffsetGlobalScaling : 1.f;

	return WeaponBobGlobalScaling*(CurrentWeaponBob.Y + CurrentJumpBob.Y)*Y + WeaponBobGlobalScaling*(CurrentWeaponBob.Z + CurrentJumpBob.Z)*Z + CrouchEyeOffset + EyeOffsetGlobalScaling*GetTransformedEyeOffset();
}

void AUTCharacter::NotifyJumpApex()
{
	DesiredJumpBob = FVector(0.f);

	AUTBot* B = Cast<AUTBot>(Controller);
	if (B != NULL)
	{
		B->NotifyJumpApex();
	}
}

float AUTCharacter::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!ShouldTakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser))
	{
		return 0.f;
	}
	else if (Damage < 0.0f)
	{
		UE_LOG(LogUTCharacter, Warning, TEXT("TakeDamage() called with damage %i of type %s... use HealDamage() to add health"), int32(Damage), *GetNameSafe(DamageEvent.DamageTypeClass));
		return 0.0f;
	}
	else
	{
		UE_LOG(LogUTCharacter, Verbose, TEXT("%s::TakeDamage() %d Class:%s Causer:%s"), *GetName(), int32(Damage), *GetNameSafe(DamageEvent.DamageTypeClass), *GetNameSafe(DamageCauser));

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
				// we need to pull the hit info out of FDamageEvent because ModifyDamage() goes through blueprints and that doesn't correctly handle polymorphic structs
				FHitResult HitInfo;
				FVector UnusedDir;
				DamageEvent.GetBestHitInfo(this, DamageCauser, HitInfo, UnusedDir);

				Game->ModifyDamage(ResultDamage, ResultMomentum, this, EventInstigator, HitInfo, DamageCauser, DamageEvent.DamageTypeClass);
			}
			AUTInventory* HitArmor = NULL;
			ModifyDamageTaken(ResultDamage, ResultMomentum, HitArmor, DamageEvent, EventInstigator, DamageCauser);

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

			if (Game->bDamageHurtsHealth || (!Cast<AUTPlayerController>(GetController()) && (!DrivenVehicle || !Cast<AUTPlayerController>(DrivenVehicle->GetController()))))
			{
				Health -= ResultDamage;
			}
			UE_LOG(LogUTCharacter, Verbose, TEXT("%s took %d damage, %d health remaining"), *GetName(), ResultDamage, Health);

			// Let the game Score damage if it wants to
			// make sure not to count overkill damage!
			Game->ScoreDamage(ResultDamage + FMath::Min<int32>(Health, 0), Controller, EventInstigator);

			bool bIsSelfDamage = (EventInstigator == Controller && Controller != NULL);
			if (UTDamageTypeCDO != NULL)
			{
				if (UTDamageTypeCDO->bForceZMomentum && GetCharacterMovement()->MovementMode == MOVE_Walking)
				{
					ResultMomentum.Z = FMath::Max<float>(ResultMomentum.Z, UTDamageTypeCDO->ForceZMomentumPct * ResultMomentum.Size());
				}
				if (bIsSelfDamage)
				{
					if (UTDamageTypeCDO->bSelfMomentumBoostOnlyZ)
					{
						ResultMomentum.Z *= UTDamageTypeCDO->SelfMomentumBoost;
					}
					else
					{
						ResultMomentum *= UTDamageTypeCDO->SelfMomentumBoost;
					}
				}
			}
			// if Z impulse is low enough and currently walking, remove Z impulse to prevent switch to falling physics, preventing lockdown effects
			else if (GetCharacterMovement()->MovementMode == MOVE_Walking && ((UTDamageTypeCDO != NULL && UTDamageTypeCDO->bPreventWalkingZMomentum) || ResultMomentum.Z < ResultMomentum.Size() * 0.1f))
			{
				ResultMomentum.Z = 0.0f;
			}
			if (IsRagdoll())
			{
				// intentionally always apply to root because that replicates better
				GetMesh()->AddImpulseAtLocation(ResultMomentum, GetMesh()->GetComponentLocation());
			}
			else if (UTCharacterMovement != NULL)
			{
				UTCharacterMovement->AddDampedImpulse(ResultMomentum, bIsSelfDamage);
				if (UTDamageTypeCDO != NULL && UTDamageTypeCDO->WalkMovementReductionDuration > 0.0f)
				{
					SetWalkMovementReduction(UTDamageTypeCDO->WalkMovementReductionPct, UTDamageTypeCDO->WalkMovementReductionDuration);
				}
			}
			else
			{
				GetCharacterMovement()->AddImpulse(ResultMomentum, false);
			}
			NotifyTakeHit(EventInstigator, ResultDamage, ResultMomentum, HitArmor, DamageEvent);
			SetLastTakeHitInfo(ResultDamage, ResultMomentum, HitArmor, DamageEvent);
			if (Health <= 0)
			{
				Died(EventInstigator, DamageEvent);
			}
		}
		else if (IsRagdoll())
		{
			FVector HitLocation = GetMesh()->GetComponentLocation();
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
			GetMesh()->AddImpulseAtLocation(ResultMomentum, HitLocation);
			if (GetNetMode() != NM_DedicatedServer)
			{
				Health -= int32(Damage);
				// note: won't be replicated in this case since already torn off but we still need it for clientside impact effects on the corpse
				SetLastTakeHitInfo(Damage, ResultMomentum, NULL, DamageEvent);
				TSubclassOf<UUTDamageType> UTDmg(*DamageEvent.DamageTypeClass);
				if (UTDmg != NULL && UTDmg.GetDefaultObject()->ShouldGib(this))
				{
					GibExplosion();
				}
			}
		}
	
		return float(ResultDamage);
	}
}

void AUTCharacter::ModifyDamageTaken_Implementation(int32& Damage, FVector& Momentum, AUTInventory*& HitArmor, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
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
	for (TInventoryIterator<> It(this); It; ++It)
	{
		if (It->bCallDamageEvents)
		{
			It->ModifyDamageTaken(Damage, Momentum, HitArmor, DamageEvent, EventInstigator, DamageCauser);
		}
	}
}
void AUTCharacter::ModifyDamageCaused_Implementation(int32& Damage, FVector& Momentum, const FDamageEvent& DamageEvent, AActor* Victim, AController* EventInstigator, AActor* DamageCauser)
{
	Damage *= DamageScaling;
}

void AUTCharacter::SetLastTakeHitInfo(int32 Damage, const FVector& Momentum, AUTInventory* HitArmor, const FDamageEvent& DamageEvent)
{
	LastTakeHitInfo.Damage = Damage;
	LastTakeHitInfo.DamageType = DamageEvent.DamageTypeClass;
	LastTakeHitInfo.HitArmor = (HitArmor != NULL) ? HitArmor->GetClass() : NULL; // the inventory object is bOnlyRelevantToOwner and wouldn't work on other clients
	LastTakeHitInfo.Momentum = Momentum;

	FVector NewRelHitLocation(FVector::ZeroVector);
	FVector ShotDir(FVector::ZeroVector);
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		NewRelHitLocation = ((FPointDamageEvent*)&DamageEvent)->HitInfo.Location - GetActorLocation();
		ShotDir = ((FPointDamageEvent*)&DamageEvent)->ShotDirection;
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && ((FRadialDamageEvent*)&DamageEvent)->ComponentHits.Num() > 0)
	{
		NewRelHitLocation = ((FRadialDamageEvent*)&DamageEvent)->ComponentHits[0].Location - GetActorLocation();
		ShotDir = (((FRadialDamageEvent*)&DamageEvent)->ComponentHits[0].ImpactPoint - ((FRadialDamageEvent*)&DamageEvent)->Origin).GetSafeNormal();
	}
	// make sure there's a difference from the last time so replication happens
	if ((NewRelHitLocation - LastTakeHitInfo.RelHitLocation).IsNearlyZero(1.0f))
	{
		NewRelHitLocation.Z += 1.0f;
	}
	LastTakeHitInfo.RelHitLocation = NewRelHitLocation;

	// set shot rotation
	// this differs from momentum in cases of e.g. damage types that kick upwards
	FRotator ShotRot = ShotDir.Rotation();
	LastTakeHitInfo.ShotDirPitch = FRotator::CompressAxisToByte(ShotRot.Pitch);
	LastTakeHitInfo.ShotDirYaw = FRotator::CompressAxisToByte(ShotRot.Yaw);

	LastTakeHitTime = GetWorld()->TimeSeconds;
			
	PlayTakeHitEffects();
}

void AUTCharacter::PlayDamageEffects_Implementation()
{
}

void AUTCharacter::PlayTakeHitEffects_Implementation()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		bool bPlayedArmorEffect = (LastTakeHitInfo.HitArmor != NULL) ? LastTakeHitInfo.HitArmor.GetDefaultObject()->HandleArmorEffects(this) : false;
		TSubclassOf<UUTDamageType> UTDmg(*LastTakeHitInfo.DamageType);
		if (UTDmg != NULL)
		{
			UTDmg.GetDefaultObject()->PlayHitEffects(this, bPlayedArmorEffect);
		}
		// check blood effects
		if (LastTakeHitInfo.Damage > 0 && (UTDmg == NULL || UTDmg.GetDefaultObject()->bCausesBlood)) 
		{
			bool bRecentlyRendered = GetWorld()->TimeSeconds - GetLastRenderTime() < 1.0f;
			// TODO: gore setting check
			if (bRecentlyRendered && BloodEffects.Num() > 0)
			{
				UParticleSystem* Blood = BloodEffects[FMath::RandHelper(BloodEffects.Num())];
				if (Blood != NULL)
				{
					// we want the PSC 'attached' to ourselves for 1P/3P visibility yet using an absolute transform, so the GameplayStatics functions don't get the job done
					UParticleSystemComponent* PSC = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass(), this);
					PSC->bAutoDestroy = true;
					PSC->SecondsBeforeInactive = 0.0f;
					PSC->bAutoActivate = false;
					PSC->SetTemplate(Blood);
					PSC->bOverrideLODMethod = false;
					PSC->RegisterComponentWithWorld(GetWorld());
					PSC->AttachTo(GetMesh());
					PSC->SetAbsolute(true, true, true);
					PSC->SetWorldLocationAndRotation(LastTakeHitInfo.RelHitLocation + GetActorLocation(), LastTakeHitInfo.RelHitLocation.Rotation());
					PSC->SetRelativeScale3D(bPlayedArmorEffect ? FVector(0.7f) : FVector(1.f));
					PSC->ActivateSystem(true);
				}
			}
			// spawn decal
			bool bSpawnDecal = bRecentlyRendered;
			if (!bSpawnDecal)
			{
				// spawn blood decals for player locally viewed even in first person mode
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					if (It->PlayerController != NULL && It->PlayerController->GetViewTarget() == this)
					{
						bSpawnDecal = true;
						break;
					}
				}
			}
			if (bSpawnDecal)
			{
				SpawnBloodDecal(LastTakeHitInfo.RelHitLocation + GetActorLocation(), FRotator(FRotator::DecompressAxisFromByte(LastTakeHitInfo.ShotDirPitch), FRotator::DecompressAxisFromByte(LastTakeHitInfo.ShotDirYaw), 0.0f).Vector());
			}
		}
	}
}

void AUTCharacter::SpawnBloodDecal(const FVector& TraceStart, const FVector& TraceDir)
{
#if !UE_SERVER
	AUTWorldSettings* Settings = Cast<AUTWorldSettings>(GetWorldSettings());
	if (Settings != NULL)
	{
		// TODO: gore setting check
		if (BloodDecals.Num() > 0)
		{
			const FBloodDecalInfo& DecalInfo = BloodDecals[FMath::RandHelper(BloodDecals.Num())];
			if (DecalInfo.Material != NULL)
			{
				static FName NAME_BloodDecal(TEXT("BloodDecal"));
				FHitResult Hit;
				if (GetWorld()->LineTraceSingle(Hit, TraceStart, TraceStart + TraceDir * (GetCapsuleComponent()->GetUnscaledCapsuleRadius() + 200.0f), ECC_Visibility, FCollisionQueryParams(NAME_BloodDecal, false, this)) && Hit.Component->bReceivesDecals)
				{
					UDecalComponent* Decal = ConstructObject<UDecalComponent>(UDecalComponent::StaticClass(), GetWorld());
					if (Hit.Component.Get() != NULL && Hit.Component->Mobility == EComponentMobility::Movable)
					{
						Decal->SetAbsolute(false, false, true);
						Decal->AttachTo(Hit.Component.Get());
					}
					else
					{
						Decal->SetAbsolute(true, true, true);
					}
					FVector2D DecalScale = DecalInfo.BaseScale * FMath::FRandRange(DecalInfo.ScaleMultRange.X, DecalInfo.ScaleMultRange.Y);
					Decal->SetWorldScale3D(FVector(1.0f, DecalScale.X, DecalScale.Y));
					Decal->SetWorldLocation(Hit.Location);
					Decal->SetWorldRotation((-Hit.Normal).Rotation() + FRotator(0.0f, 0.0f, 360.0f * FMath::FRand()));
					Decal->SetDecalMaterial(DecalInfo.Material);
					Decal->RegisterComponentWithWorld(GetWorld());
					Settings->AddImpactEffect(Decal);
				}
			}
		}
	}
#endif
}

void AUTCharacter::NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, AUTInventory* HitArmor, const FDamageEvent& DamageEvent)
{
	if (Role == ROLE_Authority)
	{
		AUTPlayerController* InstigatedByPC = Cast<AUTPlayerController>(InstigatedBy);
		if (InstigatedByPC != NULL)
		{
			InstigatedByPC->ClientNotifyCausedHit(this, Damage);
		}
		else
		{
			AUTBot* InstigatedByBot = Cast<AUTBot>(InstigatedBy);
			if (InstigatedByBot != NULL)
			{
				InstigatedByBot->NotifyCausedHit(this, Damage);
			}
		}

		// we do the sound here instead of via PlayTakeHitEffects() so it uses RPCs instead of variable replication which is higher priority
		// (at small bandwidth cost)
		if (GetWorld()->TimeSeconds - LastPainSoundTime >= MinPainSoundInterval)
		{
			if (HitArmor != nullptr && HitArmor->ReceivedDamageSound != nullptr)
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), HitArmor->ReceivedDamageSound, this, SRT_All, false, FVector::ZeroVector, InstigatedByPC, NULL, false);
			}
			else
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), PainSound, this, SRT_All, false, FVector::ZeroVector, InstigatedByPC, NULL, false);
			}
			LastPainSoundTime = GetWorld()->TimeSeconds;
		}

		AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
		if (PC != NULL)
		{
			PC->NotifyTakeHit(InstigatedBy, Damage, Momentum, DamageEvent);
		}
		else
		{
			AUTBot* B = Cast<AUTBot>(Controller);
			if (B != NULL)
			{
				B->NotifyTakeHit(InstigatedBy, Damage, Momentum, DamageEvent);
			}
		}
	}
}

void AUTCharacter::OnRepDodgeRolling()
{
	if (UTCharacterMovement)
	{
		UTCharacterMovement->bIsDodgeRolling = bRepDodgeRolling;
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
		
		AUTRemoteRedeemer* Redeemer = Cast<AUTRemoteRedeemer>(DrivenVehicle);
		if (Redeemer != nullptr)
		{
			Redeemer->DriverLeave(true);
		}

		AController* ControllerKilled = Controller;
		if (ControllerKilled == nullptr)
		{
			ControllerKilled = Cast<AController>(GetOwner());
			if (ControllerKilled == nullptr)
			{
				if (DrivenVehicle != nullptr)
				{
					ControllerKilled = DrivenVehicle->Controller;
				}
			}
		}

		GetWorld()->GetAuthGameMode<AUTGameMode>()->Killed(EventInstigator, ControllerKilled, this, DamageEvent.DamageTypeClass);

		Health = FMath::Min<int32>(Health, 0);

		// Drop any carried objects when you die.
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		if (PS != NULL && PS->CarriedObject != NULL)
		{
			PS->CarriedObject->Drop(EventInstigator);
		}

		if (ControllerKilled != nullptr)
		{
			ControllerKilled->PawnPendingDestroy(this);
		}

		OnDied.Broadcast(EventInstigator, DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass.GetDefaultObject() : NULL);

		PlayDying();

		return true;
	}
}

void AUTCharacter::StartRagdoll()
{
	// APawn::TurnOff disables collision at the end of match, undo that.
	SetActorEnableCollision(true);

	StopFiring();
	DisallowWeaponFiring(true);
	bInRagdollRecovery = false;
	
	if (!GetMesh()->ShouldTickPose())
	{
		GetMesh()->TickAnimation(0.0f);
		GetMesh()->RefreshBoneTransforms();
		GetMesh()->UpdateComponentToWorld();
	}
	GetCharacterMovement()->ApplyAccumulatedForces(0.0f);
	GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetMesh()->SetAllBodiesNotifyRigidBodyCollision(true); // note that both the component and the body instance need this set for it to apply
	GetMesh()->UpdateKinematicBonesToPhysics(GetMesh()->GetSpaceBases(), true, true, true);
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->RefreshBoneTransforms();
	GetMesh()->SetAllBodiesPhysicsBlendWeight(1.0f);
	GetMesh()->DetachFromParent(true);
	RootComponent = GetMesh();
	GetMesh()->bGenerateOverlapEvents = true;
	GetMesh()->bShouldUpdatePhysicsVolume = true;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->DetachFromParent(false);
	GetCapsuleComponent()->AttachTo(GetMesh(), NAME_None, EAttachLocation::KeepWorldPosition);

	if (bDeferredReplicatedMovement)
	{
		OnRep_ReplicatedMovement();
		// OnRep_ReplicatedMovement() will only apply to the root body but in this case we want to apply to all bodies
		if (GetMesh()->GetBodyInstance())
		{
			GetMesh()->SetAllPhysicsLinearVelocity(GetMesh()->GetBodyInstance()->GetUnrealWorldVelocity());
		}
		else
		{
			UE_LOG(LogUTCharacter, Warning, TEXT("UTCharacter does not have a body instance!"));
		}
		bDeferredReplicatedMovement = false;
	}
	else
	{
		GetMesh()->SetAllPhysicsLinearVelocity(GetMovementComponent()->Velocity, false); 
	}

	GetCharacterMovement()->StopActiveMovement();
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

void AUTCharacter::StopRagdoll()
{
	GetCapsuleComponent()->DetachFromParent(true);
	FRotator FixedRotation = GetCapsuleComponent()->RelativeRotation;
	FixedRotation.Pitch = FixedRotation.Roll = 0.0f;
	if (Controller != NULL)
	{
		// always recover in the direction the controller is facing since turning is instant
		FixedRotation.Yaw = Controller->GetControlRotation().Yaw;
	}
	GetCapsuleComponent()->SetRelativeRotation(FixedRotation);
	GetCapsuleComponent()->SetRelativeScale3D(GetClass()->GetDefaultObject<AUTCharacter>()->GetCapsuleComponent()->RelativeScale3D);
	GetCapsuleComponent()->SetCapsuleSize(GetCapsuleComponent()->GetUnscaledCapsuleRadius(), GetCharacterMovement()->CrouchedHalfHeight);
	bIsCrouched = true;
	RootComponent = GetCapsuleComponent();

	GetMesh()->MeshComponentUpdateFlag = GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->MeshComponentUpdateFlag;
	GetMesh()->bBlendPhysics = false; // for some reason bBlendPhysics == false is the value that actually blends instead of using only physics
	GetMesh()->bGenerateOverlapEvents = false;
	GetMesh()->bShouldUpdatePhysicsVolume = false;

	// TODO: make sure cylinder is in valid position (navmesh?)
	FVector AdjustedLoc = GetActorLocation() + FVector(0.0f, 0.0f, GetCharacterMovement()->CrouchedHalfHeight);
	GetWorld()->FindTeleportSpot(this, AdjustedLoc, GetActorRotation());
	GetCapsuleComponent()->SetWorldLocation(AdjustedLoc);
	if (UTCharacterMovement)
	{
		UTCharacterMovement->NeedsClientAdjustment();
	}

	// terminate constraints on the root bone so we can move it without interference
	for (int32 i = 0; i < GetMesh()->Constraints.Num(); i++)
	{
		if (GetMesh()->Constraints[i] != NULL && (GetMesh()->GetBoneIndex(GetMesh()->Constraints[i]->ConstraintBone1) == 0 || GetMesh()->GetBoneIndex(GetMesh()->Constraints[i]->ConstraintBone2) == 0))
		{
			GetMesh()->Constraints[i]->TermConstraint();
		}
	}
	// move the root bone to where we put the capsule, then disable further physics
	if (GetMesh()->Bodies.Num() > 0)
	{
		FBodyInstance* RootBody = GetMesh()->GetBodyInstance();
		if (RootBody != NULL)
		{
			FTransform NewTransform(GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeRotation, GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeLocation);
			NewTransform *= GetCapsuleComponent()->GetComponentTransform();
			RootBody->SetBodyTransform(NewTransform, true);
			RootBody->SetInstanceSimulatePhysics(false, true);
			RootBody->PhysicsBlendWeight = 1.0f; // second parameter of SetInstanceSimulatePhysics() doesn't actually work at the moment...
			GetMesh()->SyncComponentToRBPhysics();
		}
	}

	bInRagdollRecovery = true;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AUTCharacter::PlayDying()
{
	TimeOfDeath = GetWorld()->TimeSeconds;

	SetAmbientSound(NULL);
	SetLocalAmbientSound(NULL);

	SpawnBloodDecal(GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), FVector(0.0f, 0.0f, -1.0f));
	LastDeathDecalTime = GetWorld()->TimeSeconds;

	if (Hat && Hat->GetAttachParentActor())
	{
		Hat->DetachRootComponentFromParent(true);

		Hat->OnWearerDeath(LastTakeHitInfo.DamageType);

		Hat->SetLifeSpan(7.0f);
	}

	if (LeaderHat && LeaderHat->GetAttachParentActor())
	{
		LeaderHat->DetachRootComponentFromParent(true);

		LeaderHat->OnWearerDeath(LastTakeHitInfo.DamageType);

		LeaderHat->SetLifeSpan(7.0f);
	}

	if (GetNetMode() != NM_DedicatedServer && (GetWorld()->TimeSeconds - GetLastRenderTime() < 3.0f || IsLocallyViewed()))
	{
		TSubclassOf<UUTDamageType> UTDmg(*LastTakeHitInfo.DamageType);
		if (UTDmg != NULL && UTDmg.GetDefaultObject()->ShouldGib(this))
		{
			GibExplosion();
		}
		else
		{
			if (!bFeigningDeath)
			{
				StartRagdoll();
			}
			if (UTDmg != NULL)
			{
				UTDmg.GetDefaultObject()->PlayDeathEffects(this);
			}
			// StartRagdoll() changes collision properties, which can potentially result in a new overlap -> more damage -> gib explosion -> Destroy()
			// SetTimer() has a dumb assert if the target of the function is already destroyed, so we need to check it ourselves
			if (!bPendingKillPending)
			{
				FTimerHandle TempHandle;
				GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCharacter::DeathCleanupTimer, 15.0f, false);
			}
		}
	}
	else
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SetLifeSpan(0.25f);
	}
}

void AUTCharacter::GibExplosion_Implementation()
{
	if (GibExplosionEffect != NULL)
	{
		GibExplosionEffect.GetDefaultObject()->SpawnEffect(GetWorld(), RootComponent->GetComponentTransform(), GetMesh(), this, NULL, SRT_None);
	}
	for (FName BoneName : GibExplosionBones)
	{
		SpawnGib(BoneName, *LastTakeHitInfo.DamageType);
	}

	// note: if some local PlayerController is using for a ViewTarget, leave around until they switch off to prevent camera issues
	if ((GetNetMode() == NM_Client || GetNetMode() == NM_Standalone) && !IsLocallyViewed())
	{
		Destroy();
	}
	else
	{
		// need to delay for replication
		if (IsRagdoll())
		{
			StopRagdoll();
			bInRagdollRecovery = false;
			GetMesh()->SetSimulatePhysics(false);
			GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		GetCharacterMovement()->DisableMovement();
		if (GetNetMode() == NM_DedicatedServer)
		{
			SetLifeSpan(0.25f);
		}
		else
		{
			FTimerHandle TempHandle;
			GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCharacter::DeathCleanupTimer, 1.0f, false);
		}
	}
}

void AUTCharacter::DeathCleanupTimer()
{
	if (!IsLocallyViewed() && (bHidden || GetWorld()->TimeSeconds - GetLastRenderTime() > 0.5f || GetWorld()->TimeSeconds - TimeOfDeath > MaxDeathLifeSpan))
	{
		Destroy();
	}
	else
	{
		FTimerHandle TempHandle;
		GetWorldTimerManager().SetTimer(TempHandle, this, &AUTCharacter::DeathCleanupTimer, 0.5f, false);
	}
}

void AUTCharacter::SpawnGib(FName BoneName, TSubclassOf<UUTDamageType> DmgType)
{
	if (GibClass != NULL)
	{
		FTransform SpawnPos = GetMesh()->GetSocketTransform(BoneName);
		FActorSpawnParameters Params;
		Params.bNoCollisionFail = true;
		Params.Instigator = this;
		Params.Owner = this;
		AUTGib* Gib = GetWorld()->SpawnActor<AUTGib>(GibClass, SpawnPos.GetLocation(), SpawnPos.Rotator(), Params);
		if (Gib != NULL)
		{
			Gib->BloodDecals = BloodDecals;
			Gib->BloodEffects = BloodEffects;
			Gib->SetActorScale3D(Gib->GetActorScale3D() * SpawnPos.GetScale3D());
			if (Gib->Mesh != NULL)
			{
				FVector Vel = (GetMesh() == RootComponent) ? GetMesh()->GetComponentVelocity() : GetCharacterMovement()->Velocity;
				Vel += (Gib->GetActorLocation() - GetActorLocation()).GetSafeNormal() * Vel.Size() * 0.25f;
				Gib->Mesh->SetPhysicsLinearVelocity(Vel, false);
			}
			if (DmgType != NULL)
			{
				DmgType.GetDefaultObject()->PlayGibEffects(Gib);
			}
		}
	}
}

void AUTCharacter::FeignDeath()
{
	ServerFeignDeath();
}
bool AUTCharacter::ServerFeignDeath_Validate()
{
	return true;
}
void AUTCharacter::ServerFeignDeath_Implementation()
{
	if (Role == ROLE_Authority && !IsDead())
	{
		if (bFeigningDeath)
		{
			FVector TraceOffset = FVector(0.0f, 0.0f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 1.5f);
			FCollisionQueryParams FeignDeathTrace(FName(TEXT("FeignDeath")), false);
			if (GetWorld()->TimeSeconds >= FeignDeathRecoverStartTime)
			{
				FVector ActorLocation = GetCapsuleComponent()->GetComponentLocation();
				ActorLocation.Z += GetCharacterMovement()->CrouchedHalfHeight;
				UnfeignCount++;
				FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(GetCapsuleComponent()->GetUnscaledCapsuleRadius(), GetCharacterMovement()->CrouchedHalfHeight);
				static const FName NAME_FeignTrace = FName(TEXT("FeignTrace"));
				FCollisionQueryParams CapsuleParams(NAME_FeignTrace, false, this);
				FCollisionResponseParams ResponseParam;

				// Expand in place 
				TArray<FOverlapResult> Overlaps;
				bool bEncroached = GetWorld()->OverlapMulti(Overlaps, ActorLocation, FQuat::Identity, ECC_Pawn, CapsuleShape, CapsuleParams, ResponseParam);
				if (!bEncroached)
				{
					UnfeignCount = 0;
					bFeigningDeath = false;
					PlayFeignDeath();
				}
				else if (UnfeignCount > 5)
				{
					FHitResult FakeHit(this, NULL, GetActorLocation(), GetActorRotation().Vector());
					FUTPointDamageEvent FakeDamageEvent(0, FakeHit, FVector(0, 0, 0), UUTDmgType_FeignFail::StaticClass());
					UUTGameplayStatics::UTPlaySound(GetWorld(), PainSound, this, SRT_All, false, FVector::ZeroVector, Cast<AUTPlayerController>(Controller), NULL, false);
					Died(NULL, FakeDamageEvent);
				}
				else
				{
					// nudge body in random direction
					FVector FeignNudge = FeignNudgeMag * FVector(FMath::FRand(), FMath::FRand(), 0.f).GetSafeNormal();
					FeignNudge.Z = 0.4f*FeignNudgeMag;
					GetMesh()->AddImpulseAtLocation(FeignNudge, GetMesh()->GetComponentLocation());
				}
			}
		}
		else if (GetMesh()->Bodies.Num() == 0 || GetMesh()->Bodies[0]->PhysicsBlendWeight <= 0.0f)
		{
			bFeigningDeath = true;
			FeignDeathRecoverStartTime = GetWorld()->TimeSeconds + 1.0f;
			PlayFeignDeath();
		}
	}
}
void AUTCharacter::PlayFeignDeath()
{
	if (bFeigningDeath)
	{
		DropFlag();

		if (Weapon != nullptr && Weapon->DroppedPickupClass != nullptr)
		{
			TossInventory(Weapon, FVector(FMath::FRandRange(0.0f, 200.0f), FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(0.0f, 200.0f)));
		}

		if (WeaponAttachment != nullptr)
		{
			WeaponAttachment->SetActorHiddenInGame(true);
		}

		StartRagdoll();
	}
	else
	{
		if (WeaponAttachment != nullptr)
		{
			WeaponAttachment->SetActorHiddenInGame(false);
		}

		StopRagdoll();
	}
}
void AUTCharacter::ForceFeignDeath(float MinRecoveryTime)
{
	if (!bFeigningDeath && Role == ROLE_Authority && !IsDead())
	{
		bFeigningDeath = true;
		FeignDeathRecoverStartTime = GetWorld()->TimeSeconds + MinRecoveryTime;
		PlayFeignDeath();
	}
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

	if (HolsteredWeaponAttachment != NULL)
	{
		HolsteredWeaponAttachment->Destroy();
		HolsteredWeaponAttachment = NULL;
	}

	if (Hat != nullptr && Hat->GetLifeSpan() == 0.0f)
	{
		Hat->Destroy();
		Hat = NULL;
	}
	if (LeaderHat != nullptr && LeaderHat->GetLifeSpan() == 0.0f)
	{
		LeaderHat->Destroy();
		LeaderHat = NULL;
	}
	if (Eyewear != NULL)
	{
		Eyewear->Destroy();
		Eyewear = NULL;
	}

	if (GetWorld()->GetNetMode() != NM_DedicatedServer && GEngine->GetWorldContextFromWorld(GetWorld()) != NULL) // might not be able to get world context when exiting PIE
	{
		APlayerController* PC = GEngine->GetFirstLocalPlayerController(GetWorld());
		if (PC != NULL && PC->MyHUD != NULL)
		{
			PC->MyHUD->RemovePostRenderedActor(this);
		}
	}
	if (GetCharacterMovement())
	{
		GetWorldTimerManager().ClearAllTimersForObject(GetCharacterMovement());
	}
	GetWorldTimerManager().ClearAllTimersForObject(this);
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
			// don't attenuate/spatialize sounds made by a local viewtarget
			AmbientSoundComp->bAllowSpatialization = true;
			for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
			{
				if (It->PlayerController != NULL && It->PlayerController->GetViewTarget() == this)
				{
					AmbientSoundComp->bAllowSpatialization = false;
					break;
				}
			}
			AmbientSoundComp->SetSound(AmbientSound);
		}
		if (!AmbientSoundComp->IsPlaying())
		{
			AmbientSoundComp->Play();
		}
	}
}


void AUTCharacter::SetLocalAmbientSound(USoundBase* NewAmbientSound, float SoundVolume, bool bClear)
{
	if (bClear)
	{
		if ((NewAmbientSound != NULL) && (NewAmbientSound == LocalAmbientSound))
		{
			LocalAmbientSound = NULL;
			LocalAmbientSoundUpdated();
		}
	}
	else
	{
		LocalAmbientSound = NewAmbientSound;
		LocalAmbientSoundUpdated();
		if (LocalAmbientSoundComp && LocalAmbientSound)
		{
			LocalAmbientSoundComp->SetVolumeMultiplier(SoundVolume);
		}
	}
}

void AUTCharacter::LocalAmbientSoundUpdated()
{
	if (LocalAmbientSound == NULL)
	{
		if (LocalAmbientSoundComp != NULL)
		{
			LocalAmbientSoundComp->Stop();
		}
	}
	else
	{
		if (LocalAmbientSoundComp == NULL)
		{
			LocalAmbientSoundComp = NewObject<UAudioComponent>(this);
			LocalAmbientSoundComp->bAutoDestroy = false;
			LocalAmbientSoundComp->bAutoActivate = false;
		//	LocalAmbientSoundComp->bAllowSpatialization = false;
			LocalAmbientSoundComp->AttachTo(RootComponent);
			LocalAmbientSoundComp->RegisterComponent();
		}
		if (LocalAmbientSoundComp->Sound != LocalAmbientSound)
		{
			LocalAmbientSoundComp->SetSound(LocalAmbientSound);
		}
		if (!LocalAmbientSoundComp->IsPlaying())
		{
			LocalAmbientSoundComp->Play();
		}
	}
}

void AUTCharacter::SetStatusAmbientSound(USoundBase* NewAmbientSound, float SoundVolume, float PitchMultiplier, bool bClear)
{
	if (bClear)
	{
		if ((NewAmbientSound != NULL) && (NewAmbientSound == StatusAmbientSound))
		{
			StatusAmbientSound = NULL;
			StatusAmbientSoundUpdated();
		}
	}
	else
	{
		StatusAmbientSound = NewAmbientSound;
		StatusAmbientSoundUpdated();
		if (StatusAmbientSoundComp && StatusAmbientSound)
		{
			StatusAmbientSoundComp->SetVolumeMultiplier(SoundVolume);
			//StatusAmbientSoundComp->SetPitchMultiplier(PitchMultiplier);
		}
	}
}

void AUTCharacter::StatusAmbientSoundUpdated()
{
	if (StatusAmbientSound == NULL)
	{
		if (StatusAmbientSoundComp != NULL)
		{
			StatusAmbientSoundComp->Stop();
		}
	}
	else
	{
		if (StatusAmbientSoundComp == NULL)
		{
			StatusAmbientSoundComp = NewObject<UAudioComponent>(this);
			StatusAmbientSoundComp->bAutoDestroy = false;
			StatusAmbientSoundComp->bAutoActivate = false;
			//	StatusAmbientSoundComp->bAllowSpatialization = false;
			StatusAmbientSoundComp->AttachTo(RootComponent);
			StatusAmbientSoundComp->RegisterComponent();
		}
		if (StatusAmbientSoundComp->Sound != StatusAmbientSound)
		{
			StatusAmbientSoundComp->SetSound(StatusAmbientSound);
		}
		if (!StatusAmbientSoundComp->IsPlaying())
		{
			StatusAmbientSoundComp->Play();
		}
	}
}

void AUTCharacter::StartFire(uint8 FireModeNum)
{
	UE_LOG(LogUTCharacter, Verbose, TEXT("StartFire %d"), FireModeNum);

	if (!IsLocallyControlled())
	{
		UE_LOG(LogUTCharacter, Warning, TEXT("StartFire() can only be called on the owning client"));
	}
	// when feigning death, attempting to fire gets us out of it
	else if (bFeigningDeath)
	{
		FeignDeath();
	}
	else if (Weapon != NULL && EmoteCount == 0)
	{
		Weapon->StartFire(FireModeNum);
	}
}

void AUTCharacter::StopFire(uint8 FireModeNum)
{
	if (DrivenVehicle && !DrivenVehicle->IsLocallyControlled())
	{
		UE_LOG(LogUTCharacter, Warning, TEXT("StopFire() can only be called on the owning client"));
	}
	else if (!DrivenVehicle && !IsLocallyControlled())
	{
		UE_LOG(LogUTCharacter, Warning, TEXT("StopFire() can only be called on the owning client"));
	}
	else if (Weapon != NULL)
	{
		Weapon->StopFire(FireModeNum);
	}
	else
	{
		SetPendingFire(FireModeNum, false);
	}
}

void AUTCharacter::StopFiring()
{
	for (int32 i = 0; i < PendingFire.Num(); i++)
	{
		if (PendingFire[i])
		{
			StopFire(i);
		}
	}
}

bool AUTCharacter::IsTriggerDown(uint8 FireMode)
{
	return IsPendingFire(FireMode);
}

void AUTCharacter::SetFlashLocation(const FVector& InFlashLoc, uint8 InFireMode)
{
	bLocalFlashLoc = IsLocallyControlled();
	// make sure two consecutive shots don't set the same FlashLocation as that will prevent replication and thus clients won't see the shot
	FlashLocation = ((FlashLocation - InFlashLoc).SizeSquared() >= 1.0f) ? InFlashLoc : (InFlashLoc + FVector(0.0f, 0.0f, 1.0f));
	// we reserve the zero vector to stop firing, so make sure we aren't setting a value that would replicate that way
	if (FlashLocation.IsNearlyZero(1.0f))
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
void AUTCharacter::SetFlashExtra(uint8 NewFlashExtra, uint8 InFireMode)
{
	FlashExtra = NewFlashExtra;
	FireMode = InFireMode;
	FiringExtraUpdated();
}
void AUTCharacter::ClearFiringInfo()
{
	bLocalFlashLoc = false;
	FlashLocation = FVector::ZeroVector;
	FlashCount = 0;
	FlashExtra = 0;
	FiringInfoUpdated();
}
void AUTCharacter::FiringInfoReplicated()
{
	// if we locally simulated this shot, ignore the replicated value
	if (!bLocalFlashLoc)
	{
		FiringInfoUpdated();
	}
}
void AUTCharacter::FiringInfoUpdated()
{
	// Kill any montages that might be overriding the crouch anim
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance != NULL)
	{
		AnimInstance->Montage_Stop(0.2f);
	}

	// TODO: first person spectating
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(Controller);
	if (Weapon != NULL && IsLocallyControlled() && UTPC != NULL && (bLocalFlashLoc || UTPC->GetPredictionTime() == 0.f) && !UTPC->IsBehindView())
	{
		if (!FlashLocation.IsZero())
		{
			FVector SpawnLocation;
			FRotator SpawnRotation;
			Weapon->GetImpactSpawnPosition(FlashLocation, SpawnLocation, SpawnRotation);
			Weapon->PlayImpactEffects(FlashLocation, Weapon->GetCurrentFireMode(), SpawnLocation, SpawnRotation);
		}
	}
	else if (WeaponAttachment != NULL && (!IsLocallyControlled() || UTPC == NULL || UTPC->IsBehindView()))
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
void AUTCharacter::FiringExtraUpdated()
{
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(Controller);
	if (WeaponAttachment != NULL && (!IsLocallyControlled() || UTPC == NULL || UTPC->IsBehindView()))
	{
		if (FlashExtra != 0)
		{
			WeaponAttachment->FiringExtraUpdated();
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

bool AUTCharacter::HasMaxAmmo(TSubclassOf<AUTWeapon> Type) const
{
	if (Type != NULL)
	{
		int32 Amount = 0;
		for (int32 i = 0; i < SavedAmmo.Num(); i++)
		{
			if (SavedAmmo[i].Type == Type)
			{
				Amount += SavedAmmo[i].Amount;
			}
		}
		AUTWeapon* FoundWeapon = FindInventoryType<AUTWeapon>(Type, true);
		if (FoundWeapon != NULL)
		{
			Amount += FoundWeapon->Ammo;
			return Amount >= FoundWeapon->MaxAmmo;
		}
		else
		{
			return Amount >= Type.GetDefaultObject()->MaxAmmo;
		}
	}
	else
	{
		return true; // kinda arbitrary but this will make pickups more obviously broken
	}
}

int32 AUTCharacter::GetAmmoAmount(TSubclassOf<AUTWeapon> Type) const
{
	if (Type == NULL)
	{
		return 0;
	}
	else
	{
		int32 Amount = 0;
		for (int32 i = 0; i < SavedAmmo.Num(); i++)
		{
			if (SavedAmmo[i].Type == Type)
			{
				Amount += SavedAmmo[i].Amount;
			}
		}
		AUTWeapon* FoundWeapon = FindInventoryType<AUTWeapon>(Type, true);
		if (FoundWeapon != NULL)
		{
			Amount += FoundWeapon->Ammo;
		}
		return Amount;
	}
}

void AUTCharacter::AllAmmo()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	for (TInventoryIterator<AUTWeapon> It(this); It; ++It)
	{
		It->AddAmmo(It->MaxAmmo);
	}
#endif
}

AUTInventory* AUTCharacter::K2_FindInventoryType(TSubclassOf<AUTInventory> Type, bool bExactClass) const
{
	for (TInventoryIterator<> It(this); It; ++It)
	{
		if (bExactClass ? (It->GetClass() == Type) : It->IsA(Type))
		{
			return *It;
		}
	}
	return NULL;
}

void AUTCharacter::AddInventory(AUTInventory* InvToAdd, bool bAutoActivate)
{
	if (InvToAdd != NULL)
	{
		if (InvToAdd->GetUTOwner() != NULL && InvToAdd->GetUTOwner() != this && InvToAdd->GetUTOwner()->IsInInventory(InvToAdd))
		{
			UE_LOG(UT, Warning, TEXT("AddInventory (%s): Item %s is already in %s's inventory!"), *GetName(), *InvToAdd->GetName(), *InvToAdd->GetUTOwner()->GetName());
		}
		else
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
}

void AUTCharacter::RemoveInventory(AUTInventory* InvToRemove)
{
	if (InvToRemove != NULL && InventoryList != NULL)
	{
		bool bFound = false;
		if (InvToRemove == InventoryList)
		{
			bFound = true;
			InventoryList = InventoryList->NextInventory;
		}
		else
		{
			for (AUTInventory* TestInv = InventoryList; TestInv->NextInventory != NULL; TestInv = TestInv->NextInventory)
			{
				if (TestInv->NextInventory == InvToRemove)
				{
					bFound = true;
					TestInv->NextInventory = InvToRemove->NextInventory;
					break;
				}
			}
		}
		if (!bFound)
		{
			UE_LOG(UT, Warning, TEXT("RemoveInventory (%s): Item %s was not in this character's inventory!"), *GetName(), *InvToRemove->GetName());
		}
		else
		{
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
					WeaponAttachmentClass = NULL;
					UpdateWeaponAttachment();
				}
				if (!bTearOff)
				{
					if (IsLocallyControlled())
					{
						SwitchToBestWeapon();
					}
				}
			}
			InvToRemove->Removed();
		}
	}
}

bool AUTCharacter::IsInInventory(const AUTInventory* TestInv) const
{
	// we explicitly iterate all inventory items, even those with uninitialized/unreplicated Owner here
	// to avoid weird inconsistencies where the item is in the list but we think it's free to be reassigned
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
	// If we're dumping inventory on the server, make sure pending fire doesn't get stuck
	ClearPendingFire();

	// manually iterate here so any items in a bad state still get destroyed and aren't left around
	AUTInventory* Inv = InventoryList;
	while (Inv != NULL)
	{
		AUTInventory* NextInv = Inv->GetNext();
		Inv->Destroy();
		Inv = NextInv;
	}
	Weapon = NULL;
	SavedAmmo.Empty();
}

void AUTCharacter::InventoryEvent(FName EventName)
{
	for (TInventoryIterator<> It(this); It; ++It)
	{
		if (It->bCallOwnerEvent)
		{
			It->OwnerEvent(EventName);
		}
	}
}

void AUTCharacter::SwitchWeapon(AUTWeapon* NewWeapon)
{
	if (NewWeapon != NULL && !IsDead())
	{
		if (Role == ROLE_Authority)
		{
			ClientSwitchWeapon(NewWeapon);
			// NOTE: we don't call LocalSwitchWeapon() here; we ping back from the client so all weapon switches are lead by the client
			//		otherwise, we get inconsistencies if both sides trigger weapon changes
		}
		else if (!IsLocallyControlled())
		{
			UE_LOG(UT, Warning, TEXT("Illegal SwitchWeapon() call on remote client"));
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
	if (!IsDead())
	{
		// make sure clients don't try to switch to weapons that haven't been fully replicated/initialized or that have been removed and the client doesn't know yet
		if (NewWeapon != NULL && (NewWeapon->GetUTOwner() == NULL || (Role == ROLE_Authority && !IsInInventory(NewWeapon))))
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

void AUTCharacter::WeaponChanged(float OverflowTime)
{
	if (PendingWeapon != NULL && PendingWeapon->GetUTOwner() == this)
	{
		checkSlow(IsInInventory(PendingWeapon));
		Weapon = PendingWeapon;
		PendingWeapon = NULL;
		Weapon->BringUp(OverflowTime);
		WeaponClass = Weapon->GetClass();
		WeaponAttachmentClass = Weapon->AttachmentType;
		UpdateWeaponAttachment();
	}
	else if (Weapon != NULL && Weapon->GetUTOwner() == this)
	{
		// restore current weapon since pending became invalid
		Weapon->BringUp(OverflowTime);
	}
	else
	{
		Weapon = NULL;
		WeaponClass = NULL;
		WeaponAttachmentClass = NULL;
		UpdateWeaponAttachment();
	}
}

void AUTCharacter::ClientWeaponLost_Implementation(AUTWeapon* LostWeapon)
{
	if (IsLocallyControlled())
	{
		if (Weapon == LostWeapon)
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
				else
				{
					WeaponClass = NULL;
					WeaponAttachmentClass = NULL;
					UpdateWeaponAttachment();
				}
			}
		}
		else if (PendingWeapon == LostWeapon)
		{
			PendingWeapon = NULL;
			WeaponChanged();
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
		TSubclassOf<AUTWeaponAttachment> NewAttachmentClass = WeaponAttachmentClass;
		if (NewAttachmentClass == NULL)
		{
			NewAttachmentClass = (WeaponClass != NULL) ? WeaponClass.GetDefaultObject()->AttachmentType : NULL;
		}
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

void AUTCharacter::SetHolsteredWeaponAttachmentClass(TSubclassOf<AUTWeaponAttachment> NewWeaponAttachmentClass)
{
	if (Role == ROLE_Authority &&  HolsteredWeaponAttachmentClass != NewWeaponAttachmentClass)
	{
		HolsteredWeaponAttachmentClass = NewWeaponAttachmentClass;
	}
}

void AUTCharacter::UpdateHolsteredWeaponAttachment()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		TSubclassOf<AUTWeaponAttachment> NewAttachmentClass = HolsteredWeaponAttachmentClass;
		if (HolsteredWeaponAttachment != NULL && (NewAttachmentClass == NULL || (HolsteredWeaponAttachment != NULL && HolsteredWeaponAttachment->GetClass() != NewAttachmentClass)))
		{
			HolsteredWeaponAttachment->Destroy();
			HolsteredWeaponAttachment = NULL;
		}
		if (HolsteredWeaponAttachment == NULL && NewAttachmentClass != NULL)
		{
			FActorSpawnParameters Params;
			Params.Instigator = this;
			Params.Owner = this;
			HolsteredWeaponAttachment = GetWorld()->SpawnActor<AUTWeaponAttachment>(NewAttachmentClass, Params);
			HolsteredWeaponAttachment->HolsterToOwner();
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

void AUTCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTCharacter, UTReplicatedMovement, COND_SimulatedOrPhysics);
	DOREPLIFETIME_CONDITION(AUTCharacter, Health, COND_None); // would be nice to bind to teammates and spectators, but that's not an option :(

	//DOREPLIFETIME_CONDITION(AUTCharacter, InventoryList, COND_OwnerOnly);
	// replicate for cases where non-owned inventory is replicated (e.g. spectators)
	// UE4 networking doesn't cause endless replication sending unserializable values like UE3 did so this shouldn't be a big deal
	DOREPLIFETIME_CONDITION(AUTCharacter, InventoryList, COND_None); 

	DOREPLIFETIME_CONDITION(AUTCharacter, FlashCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashLocation, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashExtra, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, LastTakeHitInfo, COND_Custom);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponClass, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponAttachmentClass, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, bApplyWallSlide, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, HolsteredWeaponAttachmentClass, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, DamageScaling, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireRateMultiplier, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, AmbientSound, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, CharOverlayFlags, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponOverlayFlags, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, ReplicatedBodyMaterial, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, HeadScale, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, bFeigningDeath, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, bRepDodgeRolling, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, bSpawnProtectionEligible, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, EmoteReplicationInfo, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, EmoteSpeed, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, DrivenVehicle, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, bHasHighScore, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, WalkMovementReductionPct, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, WalkMovementReductionTime, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, bInvisible, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, HeadArmorFlashCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, bIsWearingHelmet, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, HatClass, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, EyewearClass, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, HatVariant, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, EyewearVariant, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, CosmeticFlashCount, COND_Custom);
	DOREPLIFETIME_CONDITION(AUTCharacter, CosmeticSpreeCount, COND_None);
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

bool AUTCharacter::CanDodge() const
{
	return CanDodgeInternal();
}

bool AUTCharacter::CanDodgeInternal_Implementation() const
{
	return !bIsCrouched && UTCharacterMovement && UTCharacterMovement->CanDodge() && (UTCharacterMovement->Velocity.Z > -1.f * MaxSafeFallSpeed) && EmoteCount == 0;
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
		if (UTCharacterMovement->WantsSlideRoll() && UTCharacterMovement->IsMovingOnGround())
		{
			UTCharacterMovement->PerformRoll(DodgeDir);
			return true;
		}
		else if (UTCharacterMovement->PerformDodge(DodgeDir, DodgeCross))
		{
			bCanPlayWallHitSound = true;
			OnDodge(DodgeDir);
			return true;
		}
	}
	// clear all bPressedDodge, so it doesn't get replicated or saved
	//UE_LOG(UT, Warning, TEXT("Didnt really dodge"));
	if (UTCharacterMovement)
	{
		UTCharacterMovement->ClearDodgeInput();
		UTCharacterMovement->NeedsClientAdjustment();
	}
	return false;
}

bool AUTCharacter::Roll(FVector RollDir)
{
	if (CanDodge())
	{
		if (UTCharacterMovement->IsMovingOnGround())
		{
			UTCharacterMovement->PerformRoll(RollDir);
			return true;
		}
	}
	if (UTCharacterMovement)
	{
		UTCharacterMovement->NeedsClientAdjustment();
	}
	return false;
}

bool AUTCharacter::CanJumpInternal_Implementation() const
{
	return !bIsCrouched && !IsRagdoll() && !bInRagdollRecovery && UTCharacterMovement != NULL && UTCharacterMovement->CanJump();
}

void AUTCharacter::CheckJumpInput(float DeltaTime)
{
	if (UTCharacterMovement)
	{
		UTCharacterMovement->CheckJumpInput(DeltaTime);
	}
}

void AUTCharacter::ClearJumpInput()
{
	Super::ClearJumpInput();
	if (UTCharacterMovement)
	{
		//UE_LOG(UT, Warning, TEXT("Clear jump input"));
		UTCharacterMovement->ClearDodgeInput();
	}
}

void AUTCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// find out which way is forward
		const FRotator Rotation = GetControlRotation();
		FRotator YawRotation = (UTCharacterMovement && UTCharacterMovement->Is3DMovementMode()) ? Rotation : FRotator(0, Rotation.Yaw, 0);

		// add movement in forward direction
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Value);
	}
}

void AUTCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// find out which way is right
		const FRotator Rotation = GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// add movement in right direction
		AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Value);
	}
}

void AUTCharacter::MoveUp(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in up direction
		AddMovementInput(FVector(0.f,0.f,1.f), Value);
	}
}

APlayerCameraManager* AUTCharacter::GetPlayerCameraManager()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
	return PC != NULL ? PC->PlayerCameraManager : NULL;
}

void AUTCharacter::PlayFootstep(uint8 FootNum)
{
	if ((GetWorld()->TimeSeconds - LastFootstepTime < 0.1f) || bFeigningDeath || IsDead())
	{
		return;
	}
	if (FeetAreInWater())
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), WaterFootstepSound, this, SRT_IfSourceNotReplicated);
	}
	else if (IsLocallyControlled() && Cast<APlayerController>(GetController()) )
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), OwnFootstepSound, this, SRT_IfSourceNotReplicated);
	}
	else
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), FootstepSound, this, SRT_IfSourceNotReplicated);
	}
	LastFoot = FootNum;
	LastFootstepTime = GetWorld()->TimeSeconds;
}

FVector AUTCharacter::GetTransformedEyeOffset() const
{
	FRotationMatrix ViewRotMatrix = FRotationMatrix(GetViewRotation());
	FVector XTransform = ViewRotMatrix.GetScaledAxis(EAxis::X) * EyeOffset.X;
	if ((XTransform.Z > KINDA_SMALL_NUMBER) && (XTransform.Z + EyeOffset.Z + BaseEyeHeight + CrouchEyeOffset.Z > GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 12.f))
	{
		float MaxZ = FMath::Max(0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 12.f - EyeOffset.Z - BaseEyeHeight - CrouchEyeOffset.Z);
		XTransform = XTransform * MaxZ / XTransform.Z;
	}
	return XTransform + ViewRotMatrix.GetScaledAxis(EAxis::Y) * EyeOffset.Y + FVector(0.f, 0.f, EyeOffset.Z);;
}

FVector AUTCharacter::GetPawnViewLocation() const
{
	float EyeOffsetGlobalScaling = Cast<AUTPlayerController>(GetController()) ? Cast<AUTPlayerController>(GetController())->EyeOffsetGlobalScaling : 1.f;
	return GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight) + CrouchEyeOffset + EyeOffsetGlobalScaling*GetTransformedEyeOffset();
}

void AUTCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bFindCameraComponentWhenViewTarget && CharacterCameraComponent && CharacterCameraComponent->bIsActive)
	{
		// don't allow FOV override, we handle that in UTPlayerController/UTPlayerCameraManager
		float SavedFOV = OutResult.FOV;
		CharacterCameraComponent->GetCameraView(DeltaTime, OutResult);
		OutResult.FOV = SavedFOV;
		float EyeOffsetGlobalScaling = Cast<AUTPlayerController>(GetController()) ? Cast<AUTPlayerController>(GetController())->EyeOffsetGlobalScaling : 1.f;
		OutResult.Location = OutResult.Location + CrouchEyeOffset + EyeOffsetGlobalScaling*GetTransformedEyeOffset();
	}
	else
	{
		GetActorEyesViewPoint(OutResult.Location, OutResult.Rotation);
	}
}

void AUTCharacter::PlayJump_Implementation()
{
	DesiredJumpBob = WeaponJumpBob;
	TargetEyeOffset.Z = EyeOffsetJumpBob;
	UUTGameplayStatics::UTPlaySound(GetWorld(), JumpSound, this, SRT_AllButOwner);
}

void AUTCharacter::OnDodge_Implementation(const FVector &DodgeDir)
{
	FRotator TurnRot(0.f, GetActorRotation().Yaw, 0.f);
	FRotationMatrix TurnRotMatrix = FRotationMatrix(TurnRot);
	FVector Y = TurnRotMatrix.GetScaledAxis(EAxis::Y);

	DesiredJumpBob = WeaponDodgeBob;
	if ((Y | DodgeDir) > 0.6f)
	{
		DesiredJumpBob.Y *= -1.f;
	}
	else if ((Y | DodgeDir) > -0.6f)
	{
		DesiredJumpBob.Y = 0.f;
	}
	if (GetCharacterMovement() && GetCharacterMovement()->IsSwimming())
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), SwimPushSound, this, SRT_AllButOwner);
	}
	else
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), DodgeSound, this, SRT_AllButOwner);
	}
}

void AUTCharacter::OnSlide_Implementation(const FVector &SlideDir)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), DodgeRollSound, this, SRT_None);

	FRotator TurnRot(0.f, GetActorRotation().Yaw, 0.f);
	FRotationMatrix TurnRotMatrix = FRotationMatrix(TurnRot);
	FVector Y = TurnRotMatrix.GetScaledAxis(EAxis::Y);
	DesiredJumpBob = WeaponSlideBob;
	if ((Y | SlideDir) > 0.6f)
	{
		DesiredJumpBob.Y *= -1.f;
	}
	else if ((Y | SlideDir) > -0.6f)
	{
		DesiredJumpBob.Y = 0.f;
	}
}

void AUTCharacter::Landed(const FHitResult& Hit)
{
	if (!bClientUpdating)
	{
		// cause crushing damage if we fell on another character
		if (Cast<AUTCharacter>(Hit.Actor.Get()) != NULL)
		{
			float Damage = CrushingDamageFactor * GetCharacterMovement()->Velocity.Z / -100.0f;
			if (Damage >= 1.0f)
			{
				FUTPointDamageEvent DamageEvent(Damage, Hit, -GetCharacterMovement()->Velocity.GetSafeNormal(), UUTDmgType_FallingCrush::StaticClass());
				Hit.Actor->TakeDamage(Damage, DamageEvent, Controller, this);
			}
		}

		bCanPlayWallHitSound = true;
		if (Role == ROLE_Authority)
		{
			MakeNoise(FMath::Clamp<float>(GetCharacterMovement()->Velocity.Z / (MaxSafeFallSpeed * -0.5f), 0.0f, 1.0f));
		}
		DesiredJumpBob = FVector(0.f);

		// bob weapon and viewpoint on landing
		if (GetCharacterMovement()->Velocity.Z < WeaponLandBobThreshold)
		{
			DesiredJumpBob = WeaponLandBob* FMath::Min(1.f, (-1.f*GetCharacterMovement()->Velocity.Z - WeaponLandBobThreshold) / FullWeaponLandBobVelZ);
		}
		if (GetCharacterMovement()->Velocity.Z <= EyeOffsetLandBobThreshold)
		{
			TargetEyeOffset.Z = EyeOffsetLandBob * FMath::Min(1.f, (-1.f*GetCharacterMovement()->Velocity.Z - (0.8f*EyeOffsetLandBobThreshold)) / FullEyeOffsetLandBobVelZ);
		}

		TakeFallingDamage(Hit, GetCharacterMovement()->Velocity.Z);
	}
	UTCharacterMovement->OldZ = GetActorLocation().Z;

	Super::Landed(Hit);

	if (!bClientUpdating)
	{
		static FName NAME_Landed(TEXT("Landed"));
		InventoryEvent(NAME_Landed);

		LastHitBy = NULL;

		if (UTCharacterMovement && UTCharacterMovement->bIsDodgeRolling)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), DodgeRollSound, this, SRT_None);
		}
		else if (FeetAreInWater())
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), WaterEntrySound, this, SRT_None);
		}
		else
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), LandingSound, this, SRT_None);
		}

		AUTBot* B = Cast<AUTBot>(Controller);
		if (B != NULL)
		{
			B->NotifyLanded(Hit);
		}
	}
}

void AUTCharacter::MoveBlockedBy(const FHitResult& Impact) 
{
	AUTBot* B = Cast<AUTBot>(Controller);
	if (B != NULL)
	{
		B->NotifyMoveBlocked(Impact);
	}
	if (GetCharacterMovement() && (GetCharacterMovement()->MovementMode == MOVE_Falling) && (GetWorld()->GetTimeSeconds() - LastWallHitNotifyTime > 0.5f))
	{
		if (Impact.ImpactNormal.Z > 0.4f)
		{
			TakeFallingDamage(Impact, GetCharacterMovement()->Velocity.Z);
		}
		LastWallHitNotifyTime = GetWorld()->GetTimeSeconds();
	}
}

void AUTCharacter::TakeFallingDamage(const FHitResult& Hit, float FallingSpeed)
{
	if (Role == ROLE_Authority && UTCharacterMovement)
	{
		if (FallingSpeed < -1.f * MaxSafeFallSpeed && !HandleFallingDamage(FallingSpeed, Hit))
		{
			if (FeetAreInWater())
			{
				FallingSpeed += 100.f;
			}
			if (FallingSpeed < -1.f * MaxSafeFallSpeed)
			{
				float FallingDamage = -100.f * (FallingSpeed + MaxSafeFallSpeed) / MaxSafeFallSpeed;
				FallingDamage -= UTCharacterMovement->FallingDamageReduction(FallingDamage, Hit);
				if (FallingDamage >= 1.0f)
				{
					FUTPointDamageEvent DamageEvent(FallingDamage, Hit, GetCharacterMovement()->Velocity.GetSafeNormal(), UUTDmgType_Fell::StaticClass());
					TakeDamage(DamageEvent.Damage, DamageEvent, Controller, this);
				}
			}
		}
	}
}


void AUTCharacter::OnRagdollCollision(AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (IsDead())
	{
		// maybe spawn blood as the ragdoll smacks into things
		if (OtherComp != NULL && OtherActor != this && GetWorld()->TimeSeconds - LastDeathDecalTime > 0.25f && GetWorld()->TimeSeconds - GetLastRenderTime() < 1.0f)
		{
			SpawnBloodDecal(GetActorLocation(), -Hit.Normal);
			LastDeathDecalTime = GetWorld()->TimeSeconds;
		}
	}
	// cause falling damage on Z axis collisions
	else if (!bInRagdollRecovery && FMath::Abs<float>(Hit.Normal.Z) > 0.5f)
	{
		FVector MeshVelocity = GetMesh()->GetComponentVelocity();
		// physics numbers don't seem to match up... biasing towards more falling damage over less to minimize exploits
		// besides, faceplanting ought to hurt more than landing on your feet, right? :)
		MeshVelocity.Z *= 2.0f;
		if (MeshVelocity.Z < -1.f * MaxSafeFallSpeed)
		{
			FVector SavedVelocity = GetCharacterMovement()->Velocity;
			GetCharacterMovement()->Velocity = MeshVelocity;
			TakeFallingDamage(Hit, GetCharacterMovement()->Velocity.Z);
			GetCharacterMovement()->Velocity = SavedVelocity;
			// clear Z velocity on the mesh so that this collision won't happen again unless there's a new fall
			for (int32 i = 0; i < GetMesh()->Bodies.Num(); i++)
			{
				FVector Vel = GetMesh()->Bodies[i]->GetUnrealWorldVelocity();
				Vel.Z = 0.0f;
				GetMesh()->Bodies[i]->SetLinearVelocity(Vel, false);
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

void AUTCharacter::SetWeaponAttachmentClass(TSubclassOf<AUTWeaponAttachment> NewWeaponAttachmentClass)
{
	if (Role == ROLE_Authority && NewWeaponAttachmentClass != NULL && WeaponAttachmentClass != NewWeaponAttachmentClass)
	{
		WeaponAttachmentClass = NewWeaponAttachmentClass;
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
				OverlayMesh = DuplicateObject<USkeletalMeshComponent>(GetMesh(), this);
				OverlayMesh->AttachParent = NULL; // this gets copied but we don't want it to be
				{
					// TODO: scary that these get copied, need an engine solution and/or safe way to duplicate objects during gameplay
					OverlayMesh->PrimaryComponentTick = OverlayMesh->GetClass()->GetDefaultObject<USkeletalMeshComponent>()->PrimaryComponentTick;
					OverlayMesh->PostPhysicsComponentTick = OverlayMesh->GetClass()->GetDefaultObject<USkeletalMeshComponent>()->PostPhysicsComponentTick;
				}
				OverlayMesh->SetMasterPoseComponent(GetMesh());
			}
			if (!OverlayMesh->IsRegistered())
			{
				OverlayMesh->RegisterComponent();
				OverlayMesh->AttachTo(GetMesh(), NAME_None, EAttachLocation::SnapToTarget);
				OverlayMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
			}
			UMaterialInterface* FirstOverlay = GS->GetFirstOverlay(CharOverlayFlags);
			// note: MID doesn't have any safe way to change Parent at runtime, so we need to make a new one every time...
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FirstOverlay, OverlayMesh);
			// apply team color, if applicable
			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
			if (PS != NULL && PS->Team != NULL)
			{
				static FName NAME_TeamColor(TEXT("TeamColor"));
				MID->SetVectorParameterValue(NAME_TeamColor, PS->Team->TeamColor);
			}
			for (int32 i = 0; i < OverlayMesh->GetNumMaterials(); i++)
			{
				OverlayMesh->SetMaterial(i, MID);
			}
		}
	}
}

UMaterialInstanceDynamic* AUTCharacter::GetCharOverlayMI()
{
	return (OverlayMesh != NULL && OverlayMesh->IsRegistered()) ? Cast<UMaterialInstanceDynamic>(OverlayMesh->GetMaterial(0)) : NULL;
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
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); i++)
		{
			GetMesh()->SetMaterial(i, ReplicatedBodyMaterial);
		}
	}
	else
	{
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); i++)
		{
			GetMesh()->SetMaterial(i, BodyMIs.IsValidIndex(i) ? BodyMIs[i] : GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->GetMaterial(i));
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
	if (HolsteredWeaponAttachment != NULL)
	{
		HolsteredWeaponAttachment->SetSkin(ReplicatedBodyMaterial);
	}
}

void AUTCharacter::SetBodyColorFlash(const UCurveLinearColor* ColorCurve, bool bRimOnly)
{
	BodyColorFlashCurve = ColorCurve;
	BodyColorFlashElapsedTime = 0.0f;
	for (UMaterialInstanceDynamic* MI : BodyMIs)
	{
		static FName NAME_FullBodyFlashPct(TEXT("FullBodyFlashPct"));
		if (MI != NULL)
		{
			MI->SetScalarParameterValue(NAME_FullBodyFlashPct, bRimOnly ? 0.0f : 1.0f);
		}
	}
}

void AUTCharacter::UpdateBodyColorFlash(float DeltaTime)
{
	static FName NAME_HitFlashColor(TEXT("HitFlashColor"));

	BodyColorFlashElapsedTime += DeltaTime;
	float MinTime, MaxTime;
	BodyColorFlashCurve->GetTimeRange(MinTime, MaxTime);
	for (UMaterialInstanceDynamic* MI : BodyMIs)
	{
		if (MI != NULL)
		{
			if (BodyColorFlashElapsedTime > MaxTime)
			{
				BodyColorFlashCurve = NULL;
				MI->SetVectorParameterValue(NAME_HitFlashColor, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			}
			else
			{
				MI->SetVectorParameterValue(NAME_HitFlashColor, BodyColorFlashCurve->GetLinearColorValue(BodyColorFlashElapsedTime));
			}
		}
	}
}

void AUTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (GetMesh()->MeshComponentUpdateFlag >= EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered && !GetMesh()->bRecentlyRendered && !IsLocallyControlled() 
		&& GetCharacterMovement()->MovementMode == MOVE_Walking && !bFeigningDeath && !IsDead())
	{
		// TODO: currently using an arbitrary made up interval and scale factor
		float Speed = GetCharacterMovement()->Velocity.Size();
		if (Speed > 0.0f && GetWorld()->TimeSeconds - LastFootstepTime > 0.35f * GetCharacterMovement()->MaxWalkSpeed / Speed)
		{
			PlayFootstep((LastFoot + 1) & 1);
		}
	}

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		static FName NAME_SpawnProtectionPct(TEXT("SpawnProtectionPct"));
		float ShaderValue = 0.0f;
		if (bSpawnProtectionEligible && GS->SpawnProtectionTime > 0.0f)
		{
			float Pct = 1.0f - (GetWorld()->TimeSeconds - CreationTime) / GS->SpawnProtectionTime;
			if (Pct > 0.0f)
			{
				// clamp remaining time so that the final portion of the effect snaps instead of fading
				// this makes sure it's always clear that spawn protection is still active
				ShaderValue = FMath::Max(Pct, 0.25f);
				// skip spawn protection visual if local player is on same team
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					if (It->PlayerController)
					{
						if (GS->OnSameTeam(this, It->PlayerController))
						{
							ShaderValue = 0.f;
						}
						break;
					}
				}
			}
		}
		for (UMaterialInstanceDynamic* MI : BodyMIs)
		{
			if (MI != NULL)
			{
				MI->SetScalarParameterValue(NAME_SpawnProtectionPct, ShaderValue);
			}
		}
	}
	if (BodyColorFlashCurve != NULL)
	{
		UpdateBodyColorFlash(DeltaTime);
	}

	// tick ragdoll recovery
	if (!bFeigningDeath && bInRagdollRecovery)
	{
		// TODO: anim check?
		if (GetMesh()->Bodies.Num() == 0)
		{
			bInRagdollRecovery = false;
		}
		else
		{
			GetMesh()->SetAllBodiesPhysicsBlendWeight(FMath::Max(0.0f, GetMesh()->Bodies[0]->PhysicsBlendWeight - (1.0f / FMath::Max(0.01f, RagdollBlendOutTime)) * DeltaTime));
			if (GetMesh()->Bodies[0]->PhysicsBlendWeight == 0.0f)
			{
				bInRagdollRecovery = false;
			}
		}
		if (!bInRagdollRecovery)
		{
			// disable physics and make sure mesh is in the proper position
			GetMesh()->SetSimulatePhysics(false);
			GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GetMesh()->AttachTo(GetCapsuleComponent(), NAME_None, EAttachLocation::SnapToTarget);
			GetMesh()->SetRelativeLocation(GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeLocation);
			GetMesh()->SetRelativeRotation(GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeRotation);
			GetMesh()->SetRelativeScale3D(GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeScale3D);

			DisallowWeaponFiring(GetClass()->GetDefaultObject<AUTCharacter>()->bDisallowWeaponFiring);
		}
	}

	// update eyeoffset 
	if (GetCharacterMovement()->MovementMode == MOVE_Walking && !MovementBaseUtility::UseRelativeLocation(BasedMovement.MovementBase))
	{
		// smooth up/down stairs
		if (GetCharacterMovement()->bJustTeleported && (FMath::Abs(UTCharacterMovement->OldZ - GetActorLocation().Z) > GetCharacterMovement()->MaxStepHeight))
		{
			EyeOffset.Z = 0.f;
		}
		else
		{
			EyeOffset.Z += (UTCharacterMovement->OldZ - GetActorLocation().Z);
		}

		// avoid clipping
		if (CrouchEyeOffset.Z + EyeOffset.Z > GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - BaseEyeHeight - 12.f)
		{
			if (!IsLocallyControlled())
			{
				CrouchEyeOffset.Z = 0.f;
				EyeOffset.Z = FMath::Min(EyeOffset.Z, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - BaseEyeHeight); // @TODO FIXMESTEVE CONSIDER CLIP PLANE -12.f);
			}
			else
			{
				// trace and see if camera will clip
				static FName CameraClipTrace = FName(TEXT("CameraClipTrace"));
				FCollisionQueryParams Params(CameraClipTrace, false, this);
				FHitResult Hit;
				if (GetWorld()->SweepSingle(Hit, GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight), GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight) + CrouchEyeOffset + GetTransformedEyeOffset(), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(12.f), Params))
				{
					EyeOffset.Z = Hit.Location.Z - BaseEyeHeight - GetActorLocation().Z - CrouchEyeOffset.Z; 
				}
			}
		}
		else
		{
			EyeOffset.Z = FMath::Max(EyeOffset.Z, 12.f - BaseEyeHeight - GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - CrouchEyeOffset.Z);
		}
	}
	// clamp transformed offset z contribution
	FRotationMatrix ViewRotMatrix = FRotationMatrix(GetViewRotation());
	FVector XTransform = ViewRotMatrix.GetScaledAxis(EAxis::X) * EyeOffset.X;
	if ((XTransform.Z > 0.f) && (XTransform.Z + EyeOffset.Z + BaseEyeHeight + CrouchEyeOffset.Z > GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 12.f))
	{
		float MaxZ = FMath::Max(0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 12.f - EyeOffset.Z - BaseEyeHeight - CrouchEyeOffset.Z);
		EyeOffset.X *= MaxZ / XTransform.Z;
	}

	// decay offset
	float InterpTimeX = FMath::Min(1.f, EyeOffsetInterpRate.X*DeltaTime);
	float InterpTimeY = FMath::Min(1.f, EyeOffsetInterpRate.Y*DeltaTime);
	float InterpTimeZ = FMath::Min(1.f, EyeOffsetInterpRate.Z*DeltaTime);
	EyeOffset.X = (1.f - InterpTimeX)*EyeOffset.X + InterpTimeX*TargetEyeOffset.X;
	EyeOffset.Y = (1.f - InterpTimeY)*EyeOffset.Y + InterpTimeY*TargetEyeOffset.Y;
	EyeOffset.Z = (1.f - InterpTimeZ)*EyeOffset.Z + InterpTimeZ*TargetEyeOffset.Z;
	float CrouchInterpTime = FMath::Min(1.f, CrouchEyeOffsetInterpRate*DeltaTime);
	CrouchEyeOffset = (1.f - CrouchInterpTime)*CrouchEyeOffset;
	if (EyeOffset.Z > 0.f)
	{
		// faster decay if positive
		EyeOffset.Z = (1.f - InterpTimeZ)*EyeOffset.Z + InterpTimeZ*TargetEyeOffset.Z;
	}
	EyeOffset.DiagnosticCheckNaN();
	TargetEyeOffset.X *= FMath::Max(0.f, 1.f - FMath::Min(1.f, EyeOffsetDecayRate.X*DeltaTime));
	TargetEyeOffset.Y *= FMath::Max(0.f, 1.f - FMath::Min(1.f, EyeOffsetDecayRate.Y*DeltaTime));
	TargetEyeOffset.Z *= FMath::Max(0.f, 1.f - FMath::Min(1.f, EyeOffsetDecayRate.Z*DeltaTime));
	TargetEyeOffset.DiagnosticCheckNaN();
	if (IsLocallyControlled() && Cast<APlayerController>(Controller) != NULL && GetCharacterMovement()) // @TODO FIXME ALSO FOR SPECTATORS
	{
		if ((Health <= LowHealthAmbientThreshold) && (Health > 0))
		{
			float UrgencyFactor = (LowHealthAmbientThreshold - Health) / LowHealthAmbientThreshold;
			SetStatusAmbientSound(LowHealthAmbientSound, 0.5f + FMath::Clamp<float>(UrgencyFactor, 0.f, 1.f), 1.f, false);
		}
		else
		{
			SetStatusAmbientSound(LowHealthAmbientSound, 0.f, 1.f, true);
		}
		// @TODO FIXMESTEVE this should all be event driven
		if (GetCharacterMovement()->IsFalling() && (GetCharacterMovement()->Velocity.Z < FallingAmbientStartSpeed))
		{
			SetLocalAmbientSound(FallingAmbientSound, FMath::Clamp((FallingAmbientStartSpeed - GetCharacterMovement()->Velocity.Z) / (1.5f*MaxSafeFallSpeed + FallingAmbientStartSpeed), 0.f, 1.5f), false);
		}
		else
		{
			SetLocalAmbientSound(FallingAmbientSound, 0.f, true);
			if (GetCharacterMovement()->IsMovingOnGround() && (GetCharacterMovement()->Velocity.Size2D() > SprintAmbientStartSpeed))
			{
				float NewLocalAmbientVolume = FMath::Min(1.f, (GetCharacterMovement()->Velocity.Size2D() - SprintAmbientStartSpeed) / (UTCharacterMovement->SprintSpeed - SprintAmbientStartSpeed));
				LocalAmbientVolume = LocalAmbientVolume*(1.f - DeltaTime) + NewLocalAmbientVolume*DeltaTime;
				SetLocalAmbientSound(SprintAmbientSound, LocalAmbientVolume, false);
			}
			else if ((LocalAmbientSound == SprintAmbientSound) && (LocalAmbientVolume > 0.05f))
			{
				LocalAmbientVolume = LocalAmbientVolume*(1.f - DeltaTime);
				SetLocalAmbientSound(SprintAmbientSound, LocalAmbientVolume, false);
			}
			else
			{
				SetLocalAmbientSound(SprintAmbientSound, 0.f, true);
			}
		}
	}
	else
	{
		SetStatusAmbientSound(LowHealthAmbientSound, 0.f, 1.f, true);
	}

	if ((Role == ROLE_Authority) && IsInWater())
	{
		if (IsRagdoll() && GetMesh())
		{
			// apply force to fake buoyancy and fluid friction
			float FloatMag = (IsDead() || !PositionIsInWater(GetActorLocation() + FVector(0.f, 0.f, 30.f))) ? 80.f : 190.f;
			FVector FluidForce = -500.f * GetVelocity() - FVector(0.f, 0.f, FloatMag*GetWorld()->GetGravityZ());
			GetMesh()->AddForce(0.7f*FluidForce, FName((TEXT("spine_02"))));
			GetMesh()->AddForce(0.3f*FluidForce);
		}
		bool bHeadWasUnderwater = bHeadIsUnderwater;
		bHeadIsUnderwater = IsRagdoll() || HeadIsUnderWater();

		// handle being in or out of water
		if (bHeadIsUnderwater)
		{
			if (GetWorld()->GetTimeSeconds() - LastBreathTime > MaxUnderWaterTime)
			{
				if (GetWorld()->GetTimeSeconds() - LastDrownTime > 1.f)
				{
					TakeDrowningDamage();  
					LastDrownTime = GetWorld()->GetTimeSeconds();
				}
			}
		}
		else
		{
			if (bHeadWasUnderwater && (GetWorld()->GetTimeSeconds() - LastBreathTime > MaxUnderWaterTime - 5.f))
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), GaspSound, this, SRT_None);
			}
			LastBreathTime = GetWorld()->GetTimeSeconds();
		}
	}
	else
	{
		LastBreathTime = GetWorld()->GetTimeSeconds();
	}
	/*
	if (CharacterMovement && ((CharacterMovement->GetCurrentAcceleration() | CharacterMovement->Velocity) < 0.f))
	{
	UE_LOG(UT, Warning, TEXT("Position %f %f time %f"),GetActorLocation().X, GetActorLocation().Y, GetWorld()->GetTimeSeconds());
	}*/
}

bool AUTCharacter::IsInWater() const
{
	if (IsRagdoll())
	{
		return PositionIsInWater(GetActorLocation());
	}
	return (GetCharacterMovement() && GetCharacterMovement()->IsInWater());
}

bool AUTCharacter::HeadIsUnderWater() const
{
	FVector HeadLocation = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	return PositionIsInWater(HeadLocation);
}

bool AUTCharacter::FeetAreInWater() const
{
	FVector FootLocation = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return PositionIsInWater(FootLocation);
}

bool AUTCharacter::PositionIsInWater(const FVector& Position) const
{
	// check for all volumes that overlapposition
	APhysicsVolume* NewVolume = NULL;
	TArray<FOverlapResult> Hits;
	static FName NAME_PhysicsVolumeTrace = FName(TEXT("PhysicsVolumeTrace"));
	FComponentQueryParams Params(NAME_PhysicsVolumeTrace, GetOwner());
	GetWorld()->OverlapMulti(Hits, Position, FQuat::Identity, GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeSphere(0.f), Params);

	for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
	{
		const FOverlapResult& Link = Hits[HitIdx];
		APhysicsVolume* const V = Cast<APhysicsVolume>(Link.GetActor());
		if (V && (!NewVolume || (V->Priority > NewVolume->Priority)))
		{
			NewVolume = V;
		}
	}
	return (NewVolume && NewVolume->bWaterVolume);
}

void AUTCharacter::TakeDrowningDamage()
{
	FUTPointDamageEvent DamageEvent(DrowningDamagePerSecond, FHitResult(this, GetCapsuleComponent(), GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)), FVector(0.0f, 0.0f, -1.0f), UUTDmgType_Drown::StaticClass());
	TakeDamage(DrowningDamagePerSecond, DamageEvent, Controller, this);
	UUTGameplayStatics::UTPlaySound(GetWorld(), DrowningSound, this, SRT_None);
}

uint8 AUTCharacter::GetTeamNum() const
{
	const IUTTeamInterface* TeamInterface = Cast<IUTTeamInterface>(Controller);
	if (TeamInterface != NULL)
	{
		return TeamInterface->GetTeamNum();
	}
	else if (DrivenVehicle != nullptr)
	{
		const IUTTeamInterface* VehicleTeamInterface = Cast<IUTTeamInterface>(DrivenVehicle->Controller);
		if (VehicleTeamInterface != nullptr)
		{
			return VehicleTeamInterface->GetTeamNum();
		}
		else
		{
			AUTPlayerState* PS = Cast<AUTPlayerState>(DrivenVehicle->PlayerState);
			return (PS != NULL && PS->Team != NULL) ? PS->Team->TeamIndex : 255;
		}
	}
	else
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		return (PS != NULL && PS->Team != NULL) ? PS->Team->TeamIndex : 255;
	}
}

FLinearColor AUTCharacter::GetTeamColor() const
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL && PS->Team != NULL)
	{
		return PS->Team->TeamColor;
	}
	return FLinearColor::White;
}

void AUTCharacter::PawnClientRestart()
{
	if (UTCharacterMovement)
	{
		UTCharacterMovement->ResetTimers();
	}

	Super::PawnClientRestart();
}

void AUTCharacter::PossessedBy(AController* NewController)
{
	// TODO: shouldn't base class do this? APawn::Unpossessed() still does SetOwner(NULL)...
	SetOwner(NewController);

	Super::PossessedBy(NewController);
	NotifyTeamChanged();
	NewController->ClientSetRotation(GetActorRotation());
	if (UTCharacterMovement)
	{
		UTCharacterMovement->ResetTimers();
	}
}

void AUTCharacter::UnPossessed()
{
	StopFiring();

	Super::UnPossessed();
}

void AUTCharacter::OnRepDrivenVehicle()
{
	if (DrivenVehicle)
	{
		StartDriving(DrivenVehicle);
	}
}

void AUTCharacter::StartDriving(APawn* Vehicle)
{
	DrivenVehicle = Vehicle;
	StopFiring();
	if (GetCharacterMovement() != nullptr)
	{
		GetCharacterMovement()->StopActiveMovement();
	}
}

void AUTCharacter::StopDriving(APawn* Vehicle)
{
	if (DrivenVehicle == Vehicle)
	{
		DrivenVehicle = nullptr;
	}
}

void AUTCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	if (PlayerState != NULL)
	{
		NotifyTeamChanged();
	}
}

void AUTCharacter::ApplyCharacterData(TSubclassOf<AUTCharacterContent> CharType)
{
	// FIXME: TEMP FOR GDC: needed because of TeamMaterials
	TSubclassOf<AUTCharacterContent> DefaultClass = LoadClass<AUTCharacterContent>(NULL, TEXT("/Game/RestrictedAssets/Character/Malcom_New/Malcolm_New.Malcolm_New_C"), NULL, LOAD_None, NULL);

	const AUTCharacterContent* Data = (CharType != NULL) ? CharType.GetDefaultObject() : (DefaultClass != NULL ? DefaultClass.GetDefaultObject() : NULL);
	if (Data->Mesh != NULL)
	{
		FComponentReregisterContext ReregisterContext(GetMesh());
		GetMesh()->OverrideMaterials = Data->Mesh->OverrideMaterials;
		// FIXME: TEMP FOR GDC: team override materials
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		if (PS != NULL && PS->Team != NULL)
		{
			for (int32 i = FMath::Min<int32>(Data->Mesh->GetNumMaterials(), Data->TeamMaterials.Num()) - 1; i >= 0; i--)
			{
				if (Data->TeamMaterials[i] != NULL)
				{
					GetMesh()->OverrideMaterials[i] = Data->TeamMaterials[i];
				}
			}
		}
		GetMesh()->SkeletalMesh = Data->Mesh->SkeletalMesh;
		BodyMIs.Empty();
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); i++)
		{
			// FIXME: NULL check is hack for editor reimport bug breaking number of materials
			if (GetMesh()->GetMaterial(i) != NULL)
			{
				BodyMIs.Add(GetMesh()->CreateAndSetMaterialInstanceDynamic(i));
			}
		}
		GetMesh()->PhysicsAssetOverride = Data->Mesh->PhysicsAssetOverride;
		GetMesh()->RelativeScale3D = Data->Mesh->RelativeScale3D;
		GetMesh()->RelativeLocation = Data->Mesh->RelativeLocation;
		GetMesh()->RelativeRotation = Data->Mesh->RelativeRotation;
	}
}

void AUTCharacter::NotifyTeamChanged()
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL)
	{
		// FIXME: TEMP FOR GDC: always need to do this due to team override materials
		//if (PS->GetSelectedCharacter() != NULL)
		{
			ApplyCharacterData(PS->GetSelectedCharacter());
		}
		for (UMaterialInstanceDynamic* MI : BodyMIs)
		{
			if (MI != NULL)
			{
				static FName NAME_TeamColor(TEXT("TeamColor"));
				if (PS->Team != NULL)
				{
					MI->SetVectorParameterValue(NAME_TeamColor, PS->Team->TeamColor);
					// FIXME: GDC hack: material only supports two team colors at the moment...
					MI->SetScalarParameterValue(TEXT("TeamSelect"), float(PS->Team->TeamIndex));
				}
				else
				{
					// in FFA games, let the local player decide the team coloring
					/* FIXME: temporarily removed
					for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
					{
						AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
						if (PC != NULL && PC->FFAPlayerColor.A > 0.0f)
						{
							MI->SetVectorParameterValue(NAME_TeamColor, PC->FFAPlayerColor);
							// NOTE: no splitscreen support, first player wins
							break;
						}
					}*/
				}
			}
		}
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
		UUTGameplayStatics::UTPlaySound(GetWorld(), PainSound, this, SRT_All, false, FVector::ZeroVector, Cast<AUTPlayerController>(Controller), NULL, false);
		Died(NULL, FakeDamageEvent);
	}
}

bool AUTCharacter::CanPickupObject(AUTCarriedObject* PendingObject)
{
	return GetCarriedObject() == NULL && Controller != NULL && !bTearOff && !IsDead();
}

AUTCarriedObject* AUTCharacter::GetCarriedObject()
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL && PS->CarriedObject != NULL)
	{
		return PS->CarriedObject;
	}
	return NULL;
}

void AUTCharacter::CheckArmorStacking()
{
	int32 TotalArmor = 0;
	for (TInventoryIterator<AUTArmor> It(this); It; ++It)
	{
		TotalArmor += It->ArmorAmount;
	}

	// find the lowest absorption armors, and reduce them
	while (TotalArmor > MaxStackedArmor)
	{
		TotalArmor -= ReduceArmorStack(TotalArmor-MaxStackedArmor);
	}
}

int32 AUTCharacter::ReduceArmorStack(int32 Amount)
{
	AUTArmor* WorstArmor = NULL;
	for (TInventoryIterator<AUTArmor> It(this); It; ++It)
	{
		if ((WorstArmor == NULL || (It->AbsorptionPct < WorstArmor->AbsorptionPct)))
		{
			WorstArmor = *It;
		}
	}
	checkSlow(WorstArmor);
	if (WorstArmor != NULL)
	{
		int32 ReducedAmount = FMath::Min(Amount, WorstArmor->ArmorAmount);
		WorstArmor->ReduceArmor(ReducedAmount);
		return ReducedAmount;
	}
	else
	{
		return 0;
	}
}

float AUTCharacter::GetEffectiveHealthPct(bool bOnlyVisible) const
{
	int32 TotalHealth = bOnlyVisible ? HealthMax : Health;
	for (TInventoryIterator<> It(this); It; ++It)
	{
		if (It->bCallDamageEvents)
		{
			TotalHealth += It->GetEffectiveHealthModifier(bOnlyVisible);
		}
	}

	return float(TotalHealth) / float(HealthMax);
}

/** This is only here for legacy. */
void AUTCharacter::DropFlag()
{
	DropCarriedObject();
}

void AUTCharacter::DropCarriedObject()
{
	ServerDropCarriedObject();
}


void AUTCharacter::ServerDropCarriedObject_Implementation()
{
	AUTCarriedObject* Obj = GetCarriedObject();
	if (Obj != NULL)
	{
		Obj->Drop(NULL);
	}
}

bool AUTCharacter::ServerDropCarriedObject_Validate()
{
	return true;
}

void AUTCharacter::UseCarriedObject()
{
	ServerUseCarriedObject();
}

void AUTCharacter::ServerUseCarriedObject_Implementation()
{
	AUTCarriedObject* Obj = GetCarriedObject();
	if (Obj != NULL)
	{
		Obj->Use();
	}
}

bool AUTCharacter::ServerUseCarriedObject_Validate()
{
	return true;
}

void AUTCharacter::ApplyDamageMomentum(float DamageTaken, FDamageEvent const& DamageEvent, APawn* PawnInstigator, AActor* DamageCauser)
{
	UE_LOG(UT, Warning, TEXT("Use TakeDamage() instead"));
	checkSlow(false);
}

void AUTCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (IsDead())
	{
		Super::FellOutOfWorld(DmgType);
	}
	else
	{
		FHitResult FakeHit(this, NULL, GetActorLocation(), GetActorRotation().Vector());
		FUTPointDamageEvent FakeDamageEvent(0, FakeHit, FVector(0, 0, 0), DmgType.GetClass());
		UUTGameplayStatics::UTPlaySound(GetWorld(), PainSound, this, SRT_All, false, FVector::ZeroVector, Cast<AUTPlayerController>(Controller), NULL, false);
		Died(NULL, FakeDamageEvent);
	}
}

bool AUTCharacter::TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck)
{
	if (bNoCheck)
	{
		return Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);
	}

	// during teleportation, we need to change our collision to overlap potential telefrag targets instead of block
	// however, EncroachingBlockingGeometry() doesn't handle reflexivity correctly so we can't get anywhere changing our collision responses
	// instead, we must change our object type to adjust the query
	FVector TeleportStart = GetActorLocation();
	ECollisionChannel SavedObjectType = GetCapsuleComponent()->GetCollisionObjectType();
	GetCapsuleComponent()->SetCollisionObjectType(COLLISION_TELEPORTING_OBJECT);
	bool bResult = Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);
	GetCapsuleComponent()->SetCollisionObjectType(SavedObjectType);
	GetCapsuleComponent()->UpdateOverlaps(); // make sure collision object type changes didn't mess with our overlaps
	GetCharacterMovement()->bJustTeleported = bResult && !bIsATest;
	if (bResult && !bIsATest && !bClientUpdating && TeleportEffect.Num() > 0 && TeleportEffect[0] != NULL)
	{
		TSubclassOf<AUTReplicatedEmitter> PickedEffect = TeleportEffect[0];
		int32 TeamNum = GetTeamNum();
		if (TeamNum < TeleportEffect.Num() && TeleportEffect[TeamNum] != NULL)
		{
			PickedEffect = TeleportEffect[TeamNum];
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		GetWorld()->SpawnActor<AUTReplicatedEmitter>(PickedEffect, TeleportStart, GetActorRotation(), Params);
		GetWorld()->SpawnActor<AUTReplicatedEmitter>(PickedEffect, GetActorLocation(), GetActorRotation(), Params);
	}
	if (bResult && !bIsATest && UTCharacterMovement)
	{
		UTCharacterMovement->NeedsClientAdjustment();
	}
	return bResult;
}

bool AUTCharacter::CanBlockTelefrags()
{
	for (TInventoryIterator<AUTArmor> It(this); It; ++It)
	{
		if (It->ArmorType == FName(TEXT("ShieldBelt")))
		{
			return true;
		}
	}
	return false;
}

// @TODO FIXMESTEVE - why isn't this just an implementation of ReceiveActorBeginOverlap()
void AUTCharacter::OnOverlapBegin(AActor* OtherActor)
{
	if (Role == ROLE_Authority && OtherActor != this && GetCapsuleComponent()->GetCollisionObjectType() == COLLISION_TELEPORTING_OBJECT) // need to make sure this ISN'T reflexive, only teleporting Pawn should be checking for telefrags
	{
		AUTCharacter* OtherC = Cast<AUTCharacter>(OtherActor);
		if (OtherC != NULL)
		{
			AUTTeamGameMode* TeamGame = GetWorld()->GetAuthGameMode<AUTTeamGameMode>();
			if ((TeamGame == NULL || TeamGame->TeamDamagePct > 0.0f || !GetWorld()->GetGameState<AUTGameState>()->OnSameTeam(OtherC, this)) 
				&& ((OtherC->GetActorLocation() - GetActorLocation()).Size2D() < OtherC->GetCapsuleComponent()->GetUnscaledCapsuleRadius() + GetCapsuleComponent()->GetUnscaledCapsuleRadius() - MinOverlapToTelefrag))
			{
				FUTPointDamageEvent DamageEvent(100000.0f, FHitResult(this, GetCapsuleComponent(), GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)), FVector(0.0f, 0.0f, -1.0f), UUTDmgType_Telefragged::StaticClass());
				if (OtherC->CanBlockTelefrags())
				{
					DamageEvent.DamageTypeClass = UUTDmgType_BlockedTelefrag::StaticClass();
					TakeDamage(100000.0f, DamageEvent, Controller, this);
				}
				else
				{
					OtherC->TakeDamage(100000.0f, DamageEvent, Controller, this);
				}
			}
		}
		// TODO: if OtherActor is a vehicle, then we should be killed instead
	}
}

void AUTCharacter::PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if ( UTPS != NULL && PC != NULL && PC->GetPawn() != NULL && PC->GetViewTarget() != this && GetWorld()->TimeSeconds - GetLastRenderTime() < 1.0f &&
		FVector::DotProduct(CameraDir, (GetActorLocation() - CameraPosition)) > 0.0f && GS != NULL)
	{
		float Dist = (CameraPosition - GetActorLocation()).Size();
		if (GS->OnSameTeam(PC->GetPawn(), this) && Dist <= TeamPlayerIndicatorMaxDistance)
		{
			float XL, YL;

			float Scale = Canvas->ClipX / 1920;

			UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->MediumFont;
			Canvas->TextSize(TinyFont, PlayerState->PlayerName, XL, YL,Scale,Scale);

			FVector ScreenPosition = Canvas->Project(GetActorLocation() + (GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 1.25f) * FVector(0, 0, 1));
			float XPos = ScreenPosition.X - (XL * 0.5);
			if (XPos < Canvas->ClipX || XPos + XL < 0.0f)
			{
				// Make the team backgrounds darker
				FLinearColor TeamColor = UTPS->Team ? UTPS->Team->TeamColor : FLinearColor::White;
				TeamColor.R *= 0.24;
				TeamColor.G *= 0.24;
				TeamColor.B *= 0.24;

				Canvas->SetLinearDrawColor(TeamColor);
				Canvas->DrawTile(Canvas->DefaultTexture, XPos - 1, ScreenPosition.Y - YL - 2, XL + 2, YL - (6 * Scale), 0, 0, 1, 1);

				FCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos), FMath::TruncToFloat(Canvas->OrgY + ScreenPosition.Y - YL)), FText::FromString(PlayerState->PlayerName), TinyFont, FLinearColor::White);
				TextItem.Scale = FVector2D(Scale, Scale);
				TextItem.BlendMode = SE_BLEND_Translucent;
				TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
				Canvas->DrawItem(TextItem);
			}
		}
	}
}

void AUTCharacter::OnRepHat()
{
	if (HatClass != nullptr)
	{
		if (Hat != nullptr)
		{
			Hat->Destroy();
			Hat = nullptr;
		}
		if (LeaderHat != nullptr)
		{
			LeaderHat->Destroy();
			LeaderHat = nullptr;
		}

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.bNoCollisionFail = true;
		Params.bNoFail = true;
		Hat = GetWorld()->SpawnActor<AUTHat>(HatClass, GetActorLocation(), GetActorRotation(), Params);
		if (Hat != nullptr)
		{
			FVector HatRelativeLocation = Hat->GetRootComponent()->RelativeLocation;
			FRotator HatRelativeRotation = Hat->GetRootComponent()->RelativeRotation;
			Hat->AttachRootComponentTo(GetMesh(), FName(TEXT("HatSocket")), EAttachLocation::SnapToTarget);
			Hat->SetActorRelativeRotation(HatRelativeRotation);
			Hat->SetActorRelativeLocation(HatRelativeLocation);
			Hat->OnVariantSelected(HatVariant);

			// If replication of has high score happened before hat replication, locally update it here
			if (bHasHighScore)
			{
				HasHighScoreChanged();
			}
		}
	}
}

void AUTCharacter::OnRepEyewear()
{
	if (EyewearClass != nullptr)
	{
		if (Eyewear != NULL)
		{
			Eyewear->Destroy();
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.bNoCollisionFail = true;
		Params.bNoFail = true;
		Eyewear = GetWorld()->SpawnActor<AUTEyewear>(EyewearClass, GetActorLocation(), GetActorRotation(), Params);
		if (Eyewear != NULL)
		{
			Eyewear->AttachRootComponentTo(GetMesh(), FName(TEXT("GlassesSocket")), EAttachLocation::SnapToTarget);
			Eyewear->OnVariantSelected(EyewearVariant);
		}
	}
}

void AUTCharacter::OnRepHatVariant()
{
	if (Hat != nullptr)
	{
		Hat->OnVariantSelected(HatVariant);
	}

	if (LeaderHat != nullptr)
	{
		LeaderHat->OnVariantSelected(HatVariant);
	}
}

void AUTCharacter::OnRepEyewearVariant()
{
	if (Eyewear != nullptr)
	{
		Eyewear->OnVariantSelected(EyewearVariant);
	}
}

bool AUTCharacter::IsWearingAnyCosmetic()
{
	if (HatClass != nullptr)
	{
		return true;
	}

	if (EyewearClass != nullptr)
	{
		return true;
	}

	return false;
}

void AUTCharacter::OnRepCosmeticFlashCount()
{
	if (Hat)
	{
		Hat->OnFlashCountIncremented();
	}
}

void AUTCharacter::OnRepCosmeticSpreeCount()
{
	if (Hat)
	{
		Hat->OnSpreeLevelChanged(CosmeticSpreeCount);
	}
}

bool AUTCharacter::ServerFasterEmote_Validate()
{
	return true;
}

void AUTCharacter::ServerFasterEmote_Implementation()
{
	EmoteSpeed = FMath::Min(EmoteSpeed + 0.25f, 3.0f);	
	OnRepEmoteSpeed();
}

bool AUTCharacter::ServerSlowerEmote_Validate()
{
	return true;
}

void AUTCharacter::ServerSlowerEmote_Implementation()
{
	EmoteSpeed = FMath::Max(EmoteSpeed - 0.25f, 0.0f);
	OnRepEmoteSpeed();
}

bool AUTCharacter::ServerSetEmoteSpeed_Validate(float NewEmoteSpeed)
{
	return true;
}

void AUTCharacter::ServerSetEmoteSpeed_Implementation(float NewEmoteSpeed)
{
	EmoteSpeed = FMath::Clamp(NewEmoteSpeed, 0.0f, 3.0f);
	OnRepEmoteSpeed();
}

void AUTCharacter::OnRepEmoteSpeed()
{
	if (CurrentEmote)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			AnimInstance->Montage_SetPlayRate(CurrentEmote, EmoteSpeed);
		}
	}
}

void AUTCharacter::OnRepTaunt()
{
	PlayTaunt();
}

void AUTCharacter::PlayTaunt()
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	if (UTPS != nullptr && UTPS->TauntClass != nullptr && !bFeigningDeath)
	{
		EmoteReplicationInfo.EmoteIndex = 0;
		if (Role == ROLE_Authority)
		{
			EmoteReplicationInfo.EmoteCount++;
		}

		StopFiring();
		if (Hat != NULL)
		{
			Hat->OnWearerEmoteStarted();
		}

		GetMesh()->bPauseAnims = false;
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			if (AnimInstance->Montage_Play(UTPS->TauntClass->GetDefaultObject<AUTTaunt>()->TauntMontage, EmoteSpeed))
			{
				UTCharacterMovement->bIsEmoting = true;

				CurrentEmote = UTPS->TauntClass->GetDefaultObject<AUTTaunt>()->TauntMontage;
				EmoteCount++;

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &AUTCharacter::OnEmoteEnded);
				AnimInstance->Montage_SetEndDelegate(EndDelegate);
			}			
		}
	}
}

void AUTCharacter::PlayTauntByClass(TSubclassOf<AUTTaunt> TauntToPlay)
{
	if (!bFeigningDeath && TauntToPlay != nullptr && TauntToPlay->GetDefaultObject<AUTTaunt>()->TauntMontage)
	{
		EmoteReplicationInfo.EmoteIndex = 0;
		if (Role == ROLE_Authority)
		{
			EmoteReplicationInfo.EmoteCount++;
		}

		StopFiring();
		if (Hat)
		{
			Hat->OnWearerEmoteStarted();
		}

		GetMesh()->bPauseAnims = false;
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			if (AnimInstance->Montage_Play(TauntToPlay->GetDefaultObject<AUTTaunt>()->TauntMontage, EmoteSpeed))
			{
				UTCharacterMovement->bIsEmoting = true;

				CurrentEmote = TauntToPlay->GetDefaultObject<AUTTaunt>()->TauntMontage;
				EmoteCount++;

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &AUTCharacter::OnEmoteEnded);
				AnimInstance->Montage_SetEndDelegate(EndDelegate);
			}
		}
	}
}

void AUTCharacter::OnEmoteEnded(UAnimMontage* Montage, bool bInterrupted)
{
	EmoteCount--;
	if (EmoteCount == 0)
	{
		if (Hat)
		{
			Hat->OnWearerEmoteEnded();
		}

		CurrentEmote = nullptr;
		UTCharacterMovement->bIsEmoting = false;
	}
}

EAllowedSpecialMoveAnims AUTCharacter::AllowedSpecialMoveAnims()
{
	// All emotes are full body at the moment and we're having issues with remote clients not seeing full body emotes
	/*
	if (CharacterMovement != NULL && (!CharacterMovement->IsMovingOnGround() || !CharacterMovement->GetCurrentAcceleration().IsNearlyZero()))
	{
		return EASM_UpperBodyOnly;
	}
	*/

	return EASM_Any;
}

float AUTCharacter::GetRemoteViewPitch()
{
	float ClampedPitch = (RemoteViewPitch * 360.0 / 255.0);
	ClampedPitch = ClampedPitch > 90 ? ClampedPitch - 360 : ClampedPitch;
	return FMath::Clamp<float>(ClampedPitch, -90.0, 90.0);
}

void AUTCharacter::UTUpdateSimulatedPosition(const FVector & NewLocation, const FRotator & NewRotation, const FVector& NewVelocity)
{
	if (UTCharacterMovement)
	{
		UTCharacterMovement->SimulatedVelocity = NewVelocity;

		// Always consider Location as changed if we were spawned this tick as in that case our replicated Location was set as part of spawning, before PreNetReceive()
		if ((NewLocation != GetActorLocation()) || (CreationTime == GetWorld()->TimeSeconds))
		{
			FVector FinalLocation = NewLocation;
			if (GetWorld()->EncroachingBlockingGeometry(this, NewLocation, NewRotation))
			{
				bSimGravityDisabled = true;
			}
			else
			{
				bSimGravityDisabled = false;
			}

			// Don't use TeleportTo(), that clears our base.
			SetActorLocationAndRotation(FinalLocation, NewRotation, false);
			//DrawDebugSphere(GetWorld(), FinalLocation, 30.f, 8, FColor::Red);
			if (GetCharacterMovement())
			{
				GetCharacterMovement()->bJustTeleported = true;
				//check(CharacterMovement->Velocity == NewVelocity);

				// forward simulate this character to match estimated current position on server, based on my ping
				AUTPlayerController* PC = Cast<AUTPlayerController>(GEngine->GetFirstLocalPlayerController(GetWorld()));
				float PredictionTime = PC ? PC->GetPredictionTime() : 0.f;
				if (PredictionTime > 0.f)
				{
					GetCharacterMovement()->SimulateMovement(PredictionTime);
				}
			}
		}
		else if (NewRotation != GetActorRotation())
		{
			GetRootComponent()->MoveComponent(FVector::ZeroVector, NewRotation, false);
		}
	}
}

void AUTCharacter::PostNetReceiveLocationAndRotation()
{
	if (Role == ROLE_SimulatedProxy)
	{
		// Don't change transform if using relative position (it should be nearly the same anyway, or base may be slightly out of sync)
		if (!ReplicatedBasedMovement.HasRelativeLocation())
		{
			const FVector OldLocation = GetActorLocation();
			UTUpdateSimulatedPosition(ReplicatedMovement.Location, ReplicatedMovement.Rotation, ReplicatedMovement.LinearVelocity);

			INetworkPredictionInterface* PredictionInterface = Cast<INetworkPredictionInterface>(GetMovementComponent());
			if (PredictionInterface)
			{
				PredictionInterface->SmoothCorrection(OldLocation);
			}
		}
		else if (UTCharacterMovement)
		{
			UTCharacterMovement->SimulatedVelocity = ReplicatedMovement.LinearVelocity;
		}
	}
}

void AUTCharacter::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	if (bReplicateMovement || AttachmentReplication.AttachParent)
	{
		if (GatherUTMovement())
		{
			DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, UTReplicatedMovement, bReplicateMovement);
			DOREPLIFETIME_ACTIVE_OVERRIDE(AActor, ReplicatedMovement, false);
		}
		else
		{
			DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, UTReplicatedMovement, false);
			DOREPLIFETIME_ACTIVE_OVERRIDE(AActor, ReplicatedMovement, bReplicateMovement);
		}
	}
	else
	{
		DOREPLIFETIME_ACTIVE_OVERRIDE(AActor, ReplicatedMovement, false);
		DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, UTReplicatedMovement, false);
	}
	const FAnimMontageInstance * RootMotionMontageInstance = GetRootMotionAnimMontageInstance();

	if (RootMotionMontageInstance)
	{
		// Is position stored in local space?
		RepRootMotion.bRelativePosition = BasedMovement.HasRelativeLocation();
		RepRootMotion.bRelativeRotation = BasedMovement.HasRelativeRotation();
		RepRootMotion.Location = RepRootMotion.bRelativePosition ? BasedMovement.Location : GetActorLocation();
		RepRootMotion.Rotation = RepRootMotion.bRelativeRotation ? BasedMovement.Rotation : GetActorRotation();
		RepRootMotion.MovementBase = BasedMovement.MovementBase;
		RepRootMotion.MovementBaseBoneName = BasedMovement.BoneName;
		RepRootMotion.AnimMontage = RootMotionMontageInstance->Montage;
		RepRootMotion.Position = RootMotionMontageInstance->GetPosition();

		DOREPLIFETIME_ACTIVE_OVERRIDE(ACharacter, RepRootMotion, true);
	}
	else
	{
		RepRootMotion.Clear();
		DOREPLIFETIME_ACTIVE_OVERRIDE(ACharacter, RepRootMotion, false);
	}

	ReplicatedMovementMode = GetCharacterMovement()->PackNetworkMovementMode();
	ReplicatedBasedMovement = BasedMovement;

	// Optimization: only update and replicate these values if they are actually going to be used.
	if (BasedMovement.HasRelativeLocation())
	{
		// When velocity becomes zero, force replication so the position is updated to match the server (it may have moved due to simulation on the client).
		ReplicatedBasedMovement.bServerHasVelocity = !GetCharacterMovement()->Velocity.IsZero();

		// Make sure absolute rotations are updated in case rotation occurred after the base info was saved.
		if (!BasedMovement.HasRelativeRotation())
		{
			ReplicatedBasedMovement.Rotation = GetActorRotation();
		}
	}

	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, LastTakeHitInfo, GetWorld()->TimeSeconds - LastTakeHitTime < 1.0f);
	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, HeadArmorFlashCount, GetWorld()->TimeSeconds - LastHeadArmorFlashTime < 1.0f);

	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, CosmeticFlashCount, GetWorld()->TimeSeconds - LastCosmeticFlashTime < 1.0f);

	// @TODO FIXMESTEVE - just don't want this ever replicated
	DOREPLIFETIME_ACTIVE_OVERRIDE(ACharacter, RemoteViewPitch, false);
}

bool AUTCharacter::GatherUTMovement()
{
	UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(GetRootComponent());
	if (RootPrimComp && RootPrimComp->IsSimulatingPhysics())
	{
		FRigidBodyState RBState;
		RootPrimComp->GetRigidBodyState(RBState);
		ReplicatedMovement.FillFrom(RBState);
	}
	else if (RootComponent != NULL)
	{
		// If we are attached, don't replicate absolute position
		if (RootComponent->AttachParent != NULL)
		{
			// Networking for attachments assumes the RootComponent of the AttachParent actor. 
			// If that's not the case, we can't update this, as the client wouldn't be able to resolve the Component and would detach as a result.
			if (AttachmentReplication.AttachParent != NULL)
			{
				AttachmentReplication.LocationOffset = RootComponent->RelativeLocation;
				AttachmentReplication.RotationOffset = RootComponent->RelativeRotation;
			}
		}
		else
		{
			// @TODO FIXMESTEVE make sure not replicated to owning client!!!
			UTReplicatedMovement.Location = RootComponent->GetComponentLocation();
			UTReplicatedMovement.Rotation = RootComponent->GetComponentRotation();
			UTReplicatedMovement.Rotation.Pitch = GetControlRotation().Pitch;
			//UTReplicatedMovement.Acceleration = CharacterMovement->GetCurrentAcceleration();
			UTReplicatedMovement.LinearVelocity = GetVelocity();
			return true;
		}
	}
	return false;
}

void AUTCharacter::OnRep_UTReplicatedMovement()
{
	if (Role == ROLE_SimulatedProxy)
	{
		ReplicatedMovement.Location = UTReplicatedMovement.Location;
		ReplicatedMovement.Rotation = UTReplicatedMovement.Rotation;
		RemoteViewPitch = (uint8)(ReplicatedMovement.Rotation.Pitch * 255.f / 360.f);
		ReplicatedMovement.Rotation.Pitch = 0.f;
		ReplicatedMovement.LinearVelocity = UTReplicatedMovement.LinearVelocity;
		ReplicatedMovement.AngularVelocity = FVector(0.f);
		ReplicatedMovement.bSimulatedPhysicSleep = false;
		ReplicatedMovement.bRepPhysics = false;

		OnRep_ReplicatedMovement();
	}
}

void AUTCharacter::OnRep_ReplicatedMovement()
{
	if ((bTearOff || bFeigningDeath) && (RootComponent == NULL || !RootComponent->IsSimulatingPhysics()))
	{
		bDeferredReplicatedMovement = true;
	}
	else
	{
		Super::OnRep_ReplicatedMovement();
		if (bFeigningDeath && GetMesh()->IsSimulatingPhysics())
		{
			// making the velocity apply to all bodies is more likely to be correct
			GetMesh()->SetAllPhysicsLinearVelocity(GetMesh()->GetBodyInstance()->GetUnrealWorldVelocity());
		}
	}
}

void AUTCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if ((EmoteCount > 0) || IsFeigningDeath())
	{
		return;
	}

	static const FName NAME_GameOver = FName(TEXT("GameOver"));
	AUTPlayerController *UTPC = Cast<AUTPlayerController>(Controller);
	if (UTPC != nullptr && UTPC->GetStateName() == NAME_GameOver)
	{
		return;
	}

	Super::FaceRotation(NewControlRotation, DeltaTime);
}

bool AUTCharacter::IsFeigningDeath()
{
	return bFeigningDeath;
}

void AUTCharacter::DisallowWeaponFiring(bool bDisallowed)
{
	if (bDisallowed != bDisallowWeaponFiring)
	{
		bDisallowWeaponFiring = bDisallowed;
		if (bDisallowed && Weapon != NULL)
		{
			for (int32 i = 0; i < PendingFire.Num(); i++)
			{
				if (PendingFire[i])
				{
					StopFire(i);
				}
			}
			for (UUTWeaponStateFiring* FiringState : Weapon->FiringState)
			{
				if (FiringState != NULL)
				{
					FiringState->WeaponBecameInactive();
				}
			}
		}
	}
}

void AUTCharacter::TurnOff()
{
	DisallowWeaponFiring(true);

	if (GetMesh())
	{
		GetMesh()->TickAnimation(1.0f);
		GetMesh()->RefreshBoneTransforms();
	}

	Super::TurnOff();
}

// @TODO FIXMESTEVE no NetQuantize ClientLoc for verification of perfect synchronization
void AUTCharacter::UTServerMove_Implementation(
	float TimeStamp,
	FVector_NetQuantize InAccel,
	FVector_NetQuantize ClientLoc,
	uint8 MoveFlags,
	float ViewYaw,
	float ViewPitch,
	UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	//UE_LOG(UT, Warning, TEXT("------------------------ServerMove timestamp %f moveflags %d acceleration %f %f %f"), TimeStamp, MoveFlags, InAccel.X, InAccel.Y, InAccel.Z);
	if (UTCharacterMovement)
	{
		UTCharacterMovement->ProcessServerMove(TimeStamp, InAccel, ClientLoc, MoveFlags, ViewYaw, ViewPitch, ClientMovementBase, ClientBaseBoneName, ClientMovementMode);
	}
}

bool AUTCharacter::UTServerMove_Validate(float TimeStamp, FVector_NetQuantize InAccel, FVector_NetQuantize ClientLoc, uint8 MoveFlags, float ViewYaw, float ViewPitch, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	return true;
}

void AUTCharacter::UTServerMoveOld_Implementation
(
float OldTimeStamp,
FVector_NetQuantize OldAccel,
float OldYaw,
uint8 OldMoveFlags
)
{
	//UE_LOG(UT, Warning, TEXT("======================OLDServerMove timestamp %f moveflags %d"), OldTimeStamp, OldMoveFlags);
	if (UTCharacterMovement)
	{
		UTCharacterMovement->ProcessOldServerMove(OldTimeStamp, OldAccel, OldYaw, OldMoveFlags);
	}
}

bool AUTCharacter::UTServerMoveOld_Validate(float OldTimeStamp, FVector_NetQuantize OldAccel, float OldYaw, uint8 OldMoveFlags)
{
	return true;
}

void AUTCharacter::UTServerMoveSaved_Implementation(float TimeStamp, FVector_NetQuantize InAccel, uint8 PendingFlags, float ViewYaw, float ViewPitch)
{
	//UE_LOG(UT, Warning, TEXT("---------------------ServerMoveSaved timestamp %f flags %d acceleration %f %f %f"), TimeStamp, PendingFlags, InAccel.X, InAccel.Y, InAccel.Z);
	if (UTCharacterMovement)
	{
		UTCharacterMovement->ProcessSavedServerMove(TimeStamp, InAccel, PendingFlags, ViewYaw, ViewPitch);
	}
}

bool AUTCharacter::UTServerMoveSaved_Validate(float TimeStamp, FVector_NetQuantize InAccel, uint8 PendingFlags, float ViewYaw, float ViewPitch)
{
	return true;
}

void AUTCharacter::UTServerMoveQuick_Implementation(float TimeStamp, FVector_NetQuantize InAccel, uint8 PendingFlags)
{
	//UE_LOG(UT, Warning, TEXT("----------------------ServerMoveQuick timestamp %f flags %d"), TimeStamp, PendingFlags);
	if (UTCharacterMovement)
	{
		UTCharacterMovement->ProcessQuickServerMove(TimeStamp, InAccel, PendingFlags);
	}
}

bool AUTCharacter::UTServerMoveQuick_Validate(float TimeStamp, FVector_NetQuantize InAccel, uint8 PendingFlags)
{
	return true;
}

void AUTCharacter::OnRep_HasHighScore()
{
	HasHighScoreChanged();
}

void AUTCharacter::HasHighScoreChanged_Implementation()
{
	if (bHasHighScore)
	{
		if (LeaderHat == nullptr && Hat && Hat->LeaderHatClass && Hat->LeaderHatClass->IsChildOf(AUTHatLeader::StaticClass()))
		{
			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.Instigator = this;
			Params.bNoCollisionFail = true;
			Params.bNoFail = true;
			LeaderHat = GetWorld()->SpawnActor<AUTHatLeader>(Hat->LeaderHatClass, GetActorLocation(), GetActorRotation(), Params);
			if (LeaderHat != nullptr)
			{
				FVector HatRelativeLocation = LeaderHat->GetRootComponent()->RelativeLocation;
				FRotator HatRelativeRotation = LeaderHat->GetRootComponent()->RelativeRotation;
				LeaderHat->AttachRootComponentTo(GetMesh(), FName(TEXT("HatSocket")), EAttachLocation::SnapToTarget);
				LeaderHat->SetActorRelativeRotation(HatRelativeRotation);
				LeaderHat->SetActorRelativeLocation(HatRelativeLocation);
				LeaderHat->OnVariantSelected(HatVariant);

				Hat->SetActorHiddenInGame(true);
			}
		}
	}
	else
	{
		if (LeaderHat)
		{
			LeaderHat->Destroy();
			LeaderHat = nullptr;
		}
		if (Hat)
		{
			Hat->SetActorHiddenInGame(false);
		}
	}
}

void AUTCharacter::SetWalkMovementReduction(float InPct, float InDuration)
{
	WalkMovementReductionPct = (InDuration > 0.0f) ? InPct : 0.0f;
	WalkMovementReductionTime = InDuration;
	if (UTCharacterMovement)
	{
		UTCharacterMovement->NeedsClientAdjustment();
	}
}

void AUTCharacter::OnRep_Invisible_Implementation()
{
	if (Hat)
	{
		if (bInvisible)
		{
			Hat->SetActorHiddenInGame(bInvisible);
		}
		else if (!LeaderHat)
		{
			Hat->SetActorHiddenInGame(false);
		}
	}

	if (LeaderHat)
	{
		LeaderHat->SetActorHiddenInGame(bInvisible);
	}
}

void AUTCharacter::SetInvisible(bool bNowInvisible)
{
	bInvisible = bNowInvisible;
	if (GetNetMode() != NM_DedicatedServer)
	{
		OnRep_Invisible();
	}
}
