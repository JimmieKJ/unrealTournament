// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTCharacterMovement.h"
#include "UTWeaponAttachment.h"
#include "UnrealNetwork.h"
#include "UTDmgType_Suicide.h"
#include "UTDmgType_Fell.h"
#include "UTDmgType_Drown.h"
#include "Particles/ParticleSystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
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
#include "UTPlayerCameraManager.h"
#include "ComponentReregisterContext.h"
#include "UTMutator.h"
#include "UTRewardMessage.h"
#include "StatNames.h"
#include "UTGhostComponent.h"
#include "UTTimedPowerup.h"
#include "UTWaterVolume.h"
#include "UTLift.h"
#include "UTWeaponSkin.h"
#include "UTPickupMessage.h"
#include "UTDemoRecSpectator.h"
#include "UTGameVolume.h"
#include "UTGameMode.h"
#include "UTLineUpHelper.h"
#include "UTFlagRunGameState.h"
#include "UTRallyPoint.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "Engine/DemoNetDriver.h"

static FName NAME_HatSocket(TEXT("HatSocket"));

UUTMovementBaseInterface::UUTMovementBaseInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{}

//////////////////////////////////////////////////////////////////////////
// AUTCharacter

DEFINE_LOG_CATEGORY_STATIC(LogUTCharacter, Log, All);

AUTCharacter::AUTCharacter(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UUTCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	static ConstructorHelpers::FObjectFinder<UClass> DefaultCharContentRef(TEXT("Class'/Game/RestrictedAssets/Character/Malcom_New/Malcolm_New.Malcolm_New_C'"));
	CharacterData = DefaultCharContentRef.Object;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(40.f, 108.0f);

	// Create a CameraComponent	
	CharacterCameraComponent = ObjectInitializer.CreateDefaultSubobject<UCameraComponent>(this, TEXT("FirstPersonCamera"));
	CharacterCameraComponent->SetupAttachment(GetCapsuleComponent());
	DefaultBaseEyeHeight = 83.f;
	BaseEyeHeight = DefaultBaseEyeHeight;
	CrouchedEyeHeight = 45.f;
	DefaultCrouchedEyeHeight = 40.f;
	FloorSlideEyeHeight = 1.f;
	CharacterCameraComponent->RelativeLocation = FVector(0, 0, DefaultBaseEyeHeight); // Position the camera
	CharacterCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	FirstPersonMesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("CharacterMesh1P"));
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetupAttachment(CharacterCameraComponent);
	FirstPersonMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
	FirstPersonMesh->bReceivesDecals = false;
	FirstPersonMesh->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	FirstPersonMesh->LightingChannels.bChannel1 = true;

	GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->bEnablePhysicsOnDedicatedServer = true; // needed for feign death; death ragdoll shouldn't be invoked on server
	GetMesh()->bReceivesDecals = false;
	GetMesh()->bLightAttachmentsAsGroup = true;
	GetMesh()->LightingChannels.bChannel1 = true;
	GetMesh()->bCastCapsuleIndirectShadow = true;
	UTCharacterMovement = Cast<UUTCharacterMovement>(GetCharacterMovement());
	HealthMax = 100;
	SuperHealthMax = 199;
	DamageScaling = 1.0f;
	bDamageHurtsHealth = true;
	FireRateMultiplier = 1.0f;
	bSpawnProtectionEligible = true;
	MaxSafeFallSpeed = 2400.0f;
	FallingDamageFactor = 100.0f;
	CrushingDamageFactor = 2.0f;
	HeadScale = 1.0f;
	HeadRadius = 18.0f;
	HeadHeight = 8.0f;
	HeadBone = FName(TEXT("head"));
	UnfeignCount = 0;
	bAlwaysRelevant = true;

	BobTime = 0.f;
	WeaponBobMagnitude = FVector(0.f, 0.8f, 0.4f);
	WeaponJumpBob = FVector(0.f, 0.f, -3.6f);
	WeaponDodgeBob = FVector(0.f, 6.f, -2.5f);
	WeaponLandBob = FVector(0.f, 0.f, 10.5f);
	WeaponSlideBob = FVector(0.f, 12.f, 15.f);
	WeaponBreathingBobRate = 0.2f;
	WeaponRunningBobRate = 1.2f;
	WeaponJumpBobInterpRate = 6.5f;
	WeaponHorizontalBobInterpRate = 4.3f;
	WeaponLandBobDecayRate = 5.f;
	EyeOffset = FVector(0.f, 0.f, 0.f);
	CrouchEyeOffset = EyeOffset;
	TargetEyeOffset = EyeOffset;
	EyeOffsetInterpRate = FVector(18.f, 9.f, 9.f);
	EyeOffsetDirectRate = FVector(500.f, 500.f, 100.f);
	CrouchEyeOffsetInterpRate = 12.f;
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
	bCanPickupItems = true;
	RagdollGravityScale = 1.0f;
	bAllowGibs = true;
	RagdollCollisionBleedThreshold = 2000.0f;

	MinPainSoundInterval = 0.35f;
	LastPainSoundTime = -100.0f;

	SprintAmbientStartSpeed = 1000.f;
	FallingAmbientStartSpeed = -1300.f;
	LandEffectSpeed = 500.f;

	PrimaryActorTick.bStartWithTickEnabled = true;

	// TODO: write real relevancy checking
	NetCullDistanceSquared = 500000000.0f;

	OnActorBeginOverlap.AddDynamic(this, &AUTCharacter::OnOverlapBegin);
	GetMesh()->OnComponentHit.AddDynamic(this, &AUTCharacter::OnRagdollCollision);
	GetMesh()->BodyInstance.bNotifyRigidBodyCollision = true; // note: the set function does not work correctly for SkeletalMeshComponent when used in defaults!

	TeamPlayerIndicatorMaxDistance = 2700.0f;
	SpectatorIndicatorMaxDistance = 8000.f;
	PlayerIndicatorMaxDistance = 1200.f;
	BeaconTextScale = 1.f;
	MaxSavedPositionAge = 0.3f; // @TODO FIXMESTEVE should use server's MaxPredictionPing to determine this - note also that bots will increase this if needed to satisfy their tracking requirements
	MaxShotSynchDelay = 0.2f;

	MaxStackedArmor = 150;
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
	bIsTranslocating = false;
	LastTakeHitTime = -10000.0f;
	LastTakeHitReplicatedTime = -10000.0f;
	SlideTargetHeight = 55.f;
	bCanRally = true;

	GhostComponent = ObjectInitializer.CreateDefaultSubobject<UUTGhostComponent>(this, TEXT("GhostComp"));
	FFAColor = 0;

	MaxSpeedPctModifier = 1.0f;
	MinNetUpdateFrequency = 100.0f;
}

float AUTCharacter::GetWeaponBobScaling()
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(GetController());
	return PC ? PC->WeaponBobGlobalScaling : 1.f;
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

bool AUTCharacter::PlayWaterSound(USoundBase* WaterSound)
{
	if (WaterSound && (GetWorld()->GetTimeSeconds() - LastWaterSoundTime > MinWaterSoundInterval))
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), WaterSound, this, SRT_None);
		LastWaterSoundTime = GetWorld()->GetTimeSeconds();
		return true;
	}
	return false;
}

void AUTCharacter::OnWalkingOffLedge_Implementation(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta)
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
	// adjust MaxSavedPositionAge for bot tracking purposes
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AUTBot* B = Cast<AUTBot>(It->Get());
		if (B != NULL)
		{
			MaxSavedPositionAge = FMath::Max<float>(MaxSavedPositionAge, B->TrackingReactionTime);
		}
	}

	for (int32 i = 0; i < FootstepSounds.Num(); i++)
	{
		FootstepSoundsMap.Add(FootstepSounds[i].SurfaceType, FootstepSounds[i].Sound);
		OwnFootstepSoundsMap.Add(FootstepSounds[i].SurfaceType, FootstepSounds[i].SoundOwner);
	}

	AUTWorldSettings* Settings = Cast<AUTWorldSettings>(GetWorldSettings());
	if (GetMesh() && Settings->bUseCapsuleDirectShadowsForCharacter)
	{
		GetMesh()->bCastCapsuleDirectShadow = true;
	}

	Super::BeginPlay();

	if (!IsPendingKillPending() && (SpawnSound != nullptr))
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), SpawnSound, this, SRT_None);
	}
}

void AUTCharacter::PostInitializeComponents()
{
	MaxSpeedPctModifier = 1.0f;

	Super::PostInitializeComponents();
	if ((GetNetMode() == NM_DedicatedServer) || (GetCachedScalabilityCVars().DetailMode == 0))
	{
		if (GetMesh())
		{
			GetMesh()->bDisableClothSimulation = true;
		}
	}
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

float AUTCharacter::GetCurrentSynchTime(bool bNetDelayedShot)
{
	if (bNetDelayedShot)
	{
		const float WorldTime = GetWorld()->GetTimeSeconds();
		for (int32 i = SavedPositions.Num() - 1; i >= 0; i--)
		{
			if (SavedPositions[i].bShotSpawned)
			{
				return SavedPositions[i].TimeStamp;
			}
			if (WorldTime - SavedPositions[i].Time > MaxShotSynchDelay)
			{
				break;
			}
		}
	}
	return UTCharacterMovement ? UTCharacterMovement->GetCurrentSynchTime() : 0.f;
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
	if (GetCharacterMovement())
	{
		new(SavedPositions)FSavedPosition(GetActorLocation(), GetViewRotation(), GetCharacterMovement()->Velocity, GetCharacterMovement()->bJustTeleported, bShotSpawned, WorldTime, (UTCharacterMovement ? UTCharacterMovement->GetCurrentSynchTime() : 0.f));
	}

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

void AUTCharacter::GetSimplifiedSavedPositions(TArray<FSavedPosition>& OutPositions, bool bStopAtTeleport) const
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
		if (bStopAtTeleport)
		{
			// cut off list to only those after the most recent teleport
			for (int32 i = OutPositions.Num() - 1; i >= 0; i--)
			{
				if (OutPositions[i].bTeleported)
				{
					OutPositions.RemoveAt(0, i + 1);
					break;
				}
			}
		}
	}
}

void AUTCharacter::RecalculateBaseEyeHeight()
{
	float StartBaseEyeHeight = BaseEyeHeight;

	CrouchedEyeHeight = (UTCharacterMovement && UTCharacterMovement->bIsFloorSliding) ? FloorSlideEyeHeight : DefaultCrouchedEyeHeight;
	BaseEyeHeight = (bIsCrouched || (UTCharacterMovement && UTCharacterMovement->bIsFloorSliding)) ? CrouchedEyeHeight : DefaultBaseEyeHeight;
//	UE_LOG(UT, Warning, TEXT("Recalc To %f crouched %d sliding %d"), BaseEyeHeight, bIsCrouched, (UTCharacterMovement && UTCharacterMovement->bIsFloorSliding));

	if (BaseEyeHeight != StartBaseEyeHeight)
	{
		CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight), false);
	}
}

void AUTCharacter::Crouch(bool bClientSimulation)
{
	if (UTCharacterMovement)
	{
		UTCharacterMovement->HandleCrouchRequest();
	}
}

void AUTCharacter::UnCrouch(bool bClientSimulation)
{
	if (UTCharacterMovement)
	{
		UTCharacterMovement->HandleUnCrouchRequest();
	}
}

void AUTCharacter::UpdateCrouchedEyeHeight()
{
	float StartBaseEyeHeight = BaseEyeHeight;
	RecalculateBaseEyeHeight();
	CrouchEyeOffset.Z += StartBaseEyeHeight - BaseEyeHeight;
	CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight), false);
}

void AUTCharacter::OnEndCrouch(float HeightAdjust, float ScaledHeightAdjust)
{
	float StartBaseEyeHeight = BaseEyeHeight;
	Super::OnEndCrouch(HeightAdjust, ScaledHeightAdjust);
	CrouchEyeOffset.Z += StartBaseEyeHeight - BaseEyeHeight - HeightAdjust;
	OldZ = GetActorLocation().Z;
	CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight), false);
}

void AUTCharacter::OnStartCrouch(float HeightAdjust, float ScaledHeightAdjust)
{
	if (HeightAdjust == 0.f)
	{
		// early out - it's a crouch while already sliding
		return;
	}
	float StartBaseEyeHeight = BaseEyeHeight;
	Super::OnStartCrouch(HeightAdjust, ScaledHeightAdjust);
	CrouchEyeOffset.Z += StartBaseEyeHeight - BaseEyeHeight + HeightAdjust;
	OldZ = GetActorLocation().Z;
	CharacterCameraComponent->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight), false);

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
		AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
		if (PC != NULL && PC->IsInState(NAME_Inactive))
		{
			// respawning from dead, switch to best
			PC->SwitchToBestWeapon();
		}
		else if (PendingWeapon != NULL)
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
}

static TAutoConsoleVariable<int32> CVarDebugHeadshots(
	TEXT("p.DebugHeadshots"),
	0,
	TEXT("Debug headshot traces"),
	ECVF_Default);

FVector AUTCharacter::GetHeadLocation(float PredictionTime)
{
	// force mesh update if necessary
	if (GetMesh()->IsRegistered() && GetMesh()->MeshComponentUpdateFlag > EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones && !GetMesh()->bRecentlyRendered)
	{
		if (GetMesh()->MeshComponentUpdateFlag > EMeshComponentUpdateFlag::AlwaysTickPose)
		{
			// important to have significant time here so any transitions complete
			// FIXME: step size needs to be this small due to usage of framerate-dependent FInterpTo() in the anim blueprint
			const float Step = 0.1f;
			for (float TickTime = FMath::Min<float>(GetWorld()->TimeSeconds - GetMesh()->LastRenderTime, 1.0f); TickTime > 0.0f; TickTime -= Step)
			{
				GetMesh()->TickAnimation(FMath::Min<float>(TickTime, Step), false);
			}
		}
		GetMesh()->AnimUpdateRateParams->bSkipEvaluation = false;
		GetMesh()->AnimUpdateRateParams->bInterpolateSkippedFrames = false;
		GetMesh()->RefreshBoneTransforms();
		GetMesh()->UpdateComponentToWorld();
	}
	FVector Result = GetMesh()->GetSocketLocation(HeadBone) + FVector(0.0f, 0.0f, HeadHeight);
	
	// offset based on PredictionTime to previous position
	return Result + GetRewindLocation(PredictionTime) - GetActorLocation();
}

bool AUTCharacter::IsHeadShot(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, AUTCharacter* ShotInstigator, float PredictionTime)
{
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
	return bHeadShot;
}

bool AUTCharacter::BlockedHeadShot(FVector HitLocation, FVector ShotDirection, float WeaponHeadScaling, bool bConsumeArmor, AUTCharacter* ShotInstigator)
{
	// check for inventory items that prevent headshots
	for (TInventoryIterator<> It(this); It; ++It)
	{
		if (It->bCallDamageEvents && It->PreventHeadShot(HitLocation, ShotDirection, WeaponHeadScaling, bConsumeArmor))
		{
			return true;
		}
	}
	return false;
}

void AUTCharacter::NotifyBlockedHeadShot(AUTCharacter* ShotInstigator)
{
	HeadArmorFlashCount++;
	LastHeadArmorFlashTime = GetWorld()->GetTimeSeconds();
	if (GetNetMode() == NM_Standalone)
	{
		OnRepHeadArmorFlashCount();
	}
	HeadShotBlocked();
	if (ShotInstigator)
	{
		ShotInstigator->HeadShotBlocked();
	}
}

void AUTCharacter::OnRepHeadArmorFlashCount()
{
	// play helmet client-side hit effect 
	if (HeadArmorHitEffect != NULL)
	{
		UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>(this, UParticleSystemComponent::StaticClass());
		PSC->bAutoDestroy = true;
		PSC->SecondsBeforeInactive = 0.0f;
		PSC->bAutoActivate = false;
		PSC->SetTemplate(HeadArmorHitEffect);
		PSC->bOverrideLODMethod = false;
		PSC->RegisterComponent();
		PSC->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform, NAME_HatSocket);
		PSC->ActivateSystem(true);
	}
}

void AUTCharacter::HeadShotBlocked()
{
	// locally play on instigator a sound to clearly signify headshot block
	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetController());
	if (MyPC)
	{
		MyPC->UTClientPlaySound(HeadShotBlockedSound);
	}
}

FVector AUTCharacter::GetWeaponBobOffset(float DeltaTime, AUTWeapon* MyWeapon)
{
	FRotationMatrix RotMatrix = FRotationMatrix(GetViewRotation());
	FVector X = RotMatrix.GetScaledAxis(EAxis::X);
	FVector Y = RotMatrix.GetScaledAxis(EAxis::Y);
	FVector Z = RotMatrix.GetScaledAxis(EAxis::Z);

	float InterpTime = FMath::Min(1.f, WeaponJumpBobInterpRate*DeltaTime);
	if (!GetCharacterMovement() || (GetCharacterMovement()->IsFalling() && !bApplyWallSlide))
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
		if (UTCharacterMovement && UTCharacterMovement->bIsFloorSliding)
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
		if ((bApplyWallSlide || GetCharacterMovement()->MovementMode == MOVE_Walking) && Speed > 10.0f && !bIsCrouched && (FMath::FloorToInt(0.5f + 8.f*BobTime / PI) != FMath::FloorToInt(0.5f + 8.f*LastBobTime / PI))
			&& (GetMesh()->MeshComponentUpdateFlag >= EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered) && !GetMesh()->bRecentlyRendered)
		{
			PlayFootstep((LastFoot + 1) & 1, true);
		}
	}
	float JumpYInterp = ((DesiredJumpBob.Y == 0.f) || (DesiredJumpBob.Z == 0.f)) ? FMath::Min(1.f, WeaponJumpBobInterpRate*DeltaTime) : FMath::Min(1.f, WeaponHorizontalBobInterpRate*FMath::Abs(DesiredJumpBob.Y)*DeltaTime);
	CurrentJumpBob.X = (1.f - InterpTime)*CurrentJumpBob.X + InterpTime*DesiredJumpBob.X;
	CurrentJumpBob.Y = (1.f - JumpYInterp)*CurrentJumpBob.Y + JumpYInterp*DesiredJumpBob.Y;
	CurrentJumpBob.Z = (1.f - InterpTime)*CurrentJumpBob.Z + InterpTime*DesiredJumpBob.Z;

	AUTPlayerController* MyPC = Cast<AUTPlayerController>(GetController()); // fixmesteve use the viewer rather than the controller (when can do everywhere)
	float WeaponBobGlobalScaling = (MyWeapon ? MyWeapon->WeaponBobScaling : 1.f) * (MyPC ? MyPC->WeaponBobGlobalScaling : 1.f);
	return WeaponBobGlobalScaling*(CurrentWeaponBob.Y + CurrentJumpBob.Y)*Y + WeaponBobGlobalScaling*(CurrentWeaponBob.Z + CurrentJumpBob.Z)*Z + CrouchEyeOffset + GetTransformedEyeOffset();
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
		bool bRadialDamage = false;
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
				bRadialDamage = true;
			}
		}

		if (!IsDead())
		{
			// cache here in case lose it when killed
			AUTPlayerState* MyPS = Cast<AUTPlayerState>(PlayerState);

			// we need to pull the hit info out of FDamageEvent because ModifyDamage() goes through blueprints and that doesn't correctly handle polymorphic structs
			FHitResult HitInfo;
			{
				FVector UnusedDir;
				DamageEvent.GetBestHitInfo(this, DamageCauser, HitInfo, UnusedDir);
			}

			// note that we split the gametype query out so that it's always in a consistent place
			AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if (Game != NULL)
			{
				Game->ModifyDamage(ResultDamage, ResultMomentum, this, EventInstigator, HitInfo, DamageCauser, DamageEvent.DamageTypeClass);
			}
			if (bRadialDamage && GetController() && (EventInstigator != GetController()))
			{
				AUTProjectile* Proj = Cast<AUTProjectile>(DamageCauser);
				if (Proj)
				{
					Proj->StatsHitCredit += ResultDamage;
				}
				else if (Cast<AUTRemoteRedeemer>(DamageCauser))
				{
					Cast<AUTRemoteRedeemer>(DamageCauser)->StatsHitCredit += ResultDamage;
				}
			}
			int32 AppliedDamage = ResultDamage;
			AUTInventory* HitArmor = NULL;
			bool bApplyDamageToCharacter = ((Game && Game->bDamageHurtsHealth && bDamageHurtsHealth) || (!Cast<AUTPlayerController>(GetController()) && (!DrivenVehicle || !Cast<AUTPlayerController>(DrivenVehicle->GetController()))));
			if (bApplyDamageToCharacter)
			{
				ModifyDamageTaken(AppliedDamage, ResultDamage, ResultMomentum, HitArmor, HitInfo, EventInstigator, DamageCauser, DamageEvent.DamageTypeClass);
			}
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
					OnTakeAnyDamage.Broadcast(this, ActualDamage, DamageTypeCDO, EventInstigator, DamageCauser);
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
							OnTakePointDamage.Broadcast(this, ActualDamage, EventInstigator, PointDamageEvent->HitInfo.ImpactPoint, PointDamageEvent->HitInfo.Component.Get(), PointDamageEvent->HitInfo.BoneName, PointDamageEvent->ShotDirection, DamageTypeCDO, DamageCauser);
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

			if (bApplyDamageToCharacter)
			{
				Health -= ResultDamage;
				bWasFallingWhenDamaged = (GetCharacterMovement() != NULL && (GetCharacterMovement()->MovementMode == MOVE_Falling));
				if (Health < 0)
				{
					AppliedDamage += Health;
				}
				else if (EventInstigator && Cast<AUTPlayerState>(EventInstigator->PlayerState) && (ResultDamage > 0) && (Role == ROLE_Authority) && (EventInstigator != GetController()))
				{
					if (HitArmor)
					{
						ArmorRemovalAssists.AddUnique((AUTPlayerState*)(EventInstigator->PlayerState));
						if (ArmorAmount == 0)
						{
							HealthRemovalAssists.AddUnique((AUTPlayerState*)(EventInstigator->PlayerState));
						}
					}
					else
					{
						HealthRemovalAssists.AddUnique((AUTPlayerState*)(EventInstigator->PlayerState));
					}
				}
				if ((Health <= 0) && (ArmorAmount > 0))
				{
					ArmorAmount = 0;
					UpdateArmorOverlay();
				}
			}
			UE_LOG(LogUTCharacter, Verbose, TEXT("%s took %d damage, %d health remaining"), *GetName(), ResultDamage, Health);
			if (Game && Game->HasMatchStarted())
			{
				AUTPlayerState* EnemyPS = EventInstigator ? Cast<AUTPlayerState>(EventInstigator->PlayerState) : NULL;
				Game->ScoreDamage(AppliedDamage, MyPS, EnemyPS);

				if (EnemyPS != nullptr)
				{
					if (EnemyPS->DamageDelt.Contains(DamageEvent.DamageTypeClass))
					{
						EnemyPS->DamageDelt[DamageEvent.DamageTypeClass] += AppliedDamage;
					}
					else
					{
						EnemyPS->DamageDelt.Add(DamageEvent.DamageTypeClass, AppliedDamage);
					}
				}
			}

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
				else if (bApplyWallSlide && UTCharacterMovement && !UTCharacterMovement->WallSlideNormal.IsZero())
				{
					float MomentumMag = ResultMomentum.Size();
					ResultMomentum = MomentumMag * UTCharacterMovement->WallSlideNormal;
					bApplyWallSlide = false;
				}
			}

			if (IsRagdoll())
			{
				if (GetNetMode() != NM_Standalone)
				{
					// intentionally always apply to root because that replicates better, and damp to prevent excessive team boost
					// @TODO FIXMESTEVE - want to always apply to correct bone, and damp scaled based on mesh GetMass().
					AUTGameState* GS = EventInstigator ? Cast<AUTGameState>(GetWorld()->GetGameState()) : NULL;
					float PushScaling = (GS && GS->OnSameTeam(this, EventInstigator)) ? 0.5f : 1.f;
					GetMesh()->AddImpulseAtLocation(PushScaling*ResultMomentum, GetMesh()->GetComponentLocation());
				}
				else
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
				}
			}
			else if (UTCharacterMovement != NULL)
			{
				if ((UTCharacterMovement->bIsFloorSliding) && !ResultMomentum.IsZero()) //xxx
				{
					UTCharacterMovement->Velocity.X *= 0.5f;
					UTCharacterMovement->Velocity.Y *= 0.5f;
				}
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
			NotifyTakeHit(EventInstigator, AppliedDamage, ResultDamage, ResultMomentum, HitArmor, DamageEvent);
			SetLastTakeHitInfo(Damage, ResultDamage, ResultMomentum, HitArmor, DamageEvent);
			if (Health <= 0)
			{
				AUTPlayerState* KillerPS = EventInstigator ? Cast<AUTPlayerState>(EventInstigator->PlayerState) : NULL;
				if (KillerPS)
				{
					AUTProjectile* Proj = Cast<AUTProjectile>(DamageCauser);
					KillerPS->bAnnounceWeaponReward = Proj && Proj->bPendingSpecialReward;
				}
				Died(EventInstigator, DamageEvent, DamageCauser);
				if (KillerPS)
				{
					KillerPS->bAnnounceWeaponReward = false;
				}
			}
		}
		else
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
			if (IsRagdoll())
			{
				GetMesh()->AddImpulseAtLocation(ResultMomentum, HitLocation);
			}
			if ((GetNetMode() != NM_DedicatedServer) && !IsPendingKillPending())
			{
				Health -= int32(Damage);
				// note: won't be replicated in this case since already torn off but we still need it for clientside impact effects on the corpse
				SetLastTakeHitInfo(Damage, Damage, ResultMomentum, NULL, DamageEvent);
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

bool AUTCharacter::ModifyDamageTaken_Implementation(int32& AppliedDamage, int32& Damage, FVector& Momentum, AUTInventory*& HitArmor, const FHitResult& HitInfo, AController* EventInstigator, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
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
		InstigatorChar->ModifyDamageCaused(AppliedDamage, Damage, Momentum, HitInfo, this, EventInstigator, DamageCauser, DamageType);
	}
	// check inventory
	for (TInventoryIterator<> It(this); It; ++It)
	{
		if (It->bCallDamageEvents)
		{
			It->ModifyDamageTaken(Damage, Momentum, HitArmor, EventInstigator, HitInfo, DamageCauser, DamageType);
		}
	}
	// use armor
	int32 CurrentArmor = GetArmorAmount();
	if ((Damage > 0) && (CurrentArmor > 0))
	{
		const UDamageType* const DamageTypeCDO = (DamageType != NULL) ? DamageType->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
		const UUTDamageType* const UTDamageTypeCDO = Cast<UUTDamageType>(DamageTypeCDO); // warning: may be NULL
		if (UTDamageTypeCDO == NULL || UTDamageTypeCDO->bBlockedByArmor)
		{
			HitArmor = ArmorType;
			int32 AbsorbedDamage = 0;
			int32 InitialDamage = Damage;
			if (CurrentArmor > 100)
			{
				// absorb 100% of damage if armor > 100
				AbsorbedDamage = FMath::Min(Damage, CurrentArmor - 100);
				Damage -= AbsorbedDamage;
			}
			if (Damage > 0)
			{
				// absorb 50% of damage if armor <= 100
				int32 PartialAbsorbedDamage = FMath::Min(CurrentArmor, FMath::Max<int32>(1, Damage * 0.5f));
				Damage -= PartialAbsorbedDamage;
				AbsorbedDamage += PartialAbsorbedDamage;
			}
			if (ArmorType && ArmorType->bAbsorbMomentum)
			{
				Momentum *= 1.0f - float(AbsorbedDamage) / float(InitialDamage);
			}
			RemoveArmor(AbsorbedDamage);
		}
	}
	return false;
}

bool AUTCharacter::ModifyDamageCaused_Implementation(int32& AppliedDamage, int32& Damage, FVector& Momentum, const FHitResult& HitInfo, AActor* Victim, AController* EventInstigator, AActor* DamageCauser, TSubclassOf<UDamageType> DamageType)
{
	if (DamageType && !DamageType->GetDefaultObject<UDamageType>()->bCausedByWorld && ((Victim != this) || (DamageScaling < 1.f)))
	{
		Damage *= DamageScaling;
		AppliedDamage *= DamageScaling;
	}
	return false;
}

void AUTCharacter::SetLastTakeHitInfo(int32 AttemptedDamage, int32 Damage, const FVector& Momentum, AUTInventory* HitArmor, const FDamageEvent& DamageEvent)
{
	// if we haven't replicated a previous hit yet (generally, multi hit within same frame), stack with it
	bool bStackHit = (LastTakeHitTime > LastTakeHitReplicatedTime && DamageEvent.DamageTypeClass == LastTakeHitInfo.DamageType) && (GetNetMode() == NM_DedicatedServer);
	LastTakeHitInfo.Damage = Damage;
	LastTakeHitInfo.DamageType = DamageEvent.DamageTypeClass;
	if (!bStackHit || LastTakeHitInfo.HitArmor == NULL)
	{
		LastTakeHitInfo.HitArmor = ((HitArmor != NULL) && HitArmor->ShouldDisplayHitEffect(AttemptedDamage, Damage, Health, ArmorAmount)) ? HitArmor->GetClass() : NULL; // the inventory object is bOnlyRelevantToOwner and wouldn't work on other clients
	}
	if ((LastTakeHitInfo.HitArmor == NULL) && (HitArmor == NULL) && (Health > 0) && (Damage == AttemptedDamage) && (Health + Damage > HealthMax) && ((Damage > 90) || (Health > 90)))
	{
		// notify superhealth hit effect
		LastTakeHitInfo.HitArmor = AUTTimedPowerup::StaticClass();
	}
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

	if (bStackHit)
	{
		LastTakeHitInfo.Count++;
	}
	else
	{
		LastTakeHitInfo.Count = 1;
	}

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
		// send hit notify for spectators
		for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(It->PlayerController);
			if (PC != NULL && PC->GetViewTarget() == this && PC->GetPawn() != this)
			{
				PC->ClientNotifyTakeHit(false, FMath::Clamp(LastTakeHitInfo.Damage, 0, 255), LastTakeHitInfo.ShotDirYaw);
			}
		}

		// never play armor effect if dead, prefer blood
		bool bPlayedArmorEffect = (LastTakeHitInfo.HitArmor != NULL && !bTearOff) ? LastTakeHitInfo.HitArmor.GetDefaultObject()->HandleArmorEffects(this) : false;
		TSubclassOf<UUTDamageType> UTDmg(*LastTakeHitInfo.DamageType);
		if (UTDmg != NULL)
		{
			UTDmg.GetDefaultObject()->PlayHitEffects(this, bPlayedArmorEffect);
			if (!UTDmg.GetDefaultObject()->bCausedByWorld)
			{
				LastTakeHitTime = GetWorld()->TimeSeconds; // is set client-side if enemy caused FIXMESTEVE - need caused by weapon flag
			}
		}
		// check blood effects
		if (!bPlayedArmorEffect && LastTakeHitInfo.Damage > 0 && (UTDmg == NULL || UTDmg.GetDefaultObject()->bCausesBlood)) 
		{
			bool bRecentlyRendered = GetWorld()->TimeSeconds - GetLastRenderTime() < 1.0f;
			// TODO: gore setting check
			if (bRecentlyRendered && BloodEffects.Num() > 0)
			{
				UParticleSystem* Blood = BloodEffects[FMath::RandHelper(BloodEffects.Num())];
				if (Blood != NULL)
				{
					// we want the PSC 'attached' to ourselves for 1P/3P visibility yet using an absolute transform, so the GameplayStatics functions don't get the job done
					UParticleSystemComponent* PSC = NewObject<UParticleSystemComponent>(this, UParticleSystemComponent::StaticClass());
					PSC->bAutoDestroy = true;
					PSC->SecondsBeforeInactive = 0.0f;
					PSC->bAutoActivate = false;
					PSC->SetTemplate(Blood);
					PSC->bOverrideLODMethod = false;
					PSC->RegisterComponentWithWorld(GetWorld());
					PSC->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform);
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
				if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceStart + TraceDir * (GetCapsuleComponent()->GetUnscaledCapsuleRadius() + 200.0f), ECC_Visibility, FCollisionQueryParams(NAME_BloodDecal, false, this)) && Hit.Component->bReceivesDecals)
				{
					UDecalComponent* Decal = NewObject<UDecalComponent>(GetWorld(), UDecalComponent::StaticClass());
					if (Hit.Component.Get() != NULL && Hit.Component->Mobility == EComponentMobility::Movable)
					{
						Decal->SetAbsolute(false, false, true);
						Decal->AttachToComponent(Hit.Component.Get(), FAttachmentTransformRules::KeepRelativeTransform);
					}
					else
					{
						Decal->SetAbsolute(true, true, true);
					}
					FVector2D DecalScale = DecalInfo.BaseScale * FMath::FRandRange(DecalInfo.ScaleMultRange.X, DecalInfo.ScaleMultRange.Y);
					Decal->DecalSize = FVector(1.0f, DecalScale.X, DecalScale.Y);
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

void AUTCharacter::TargetedBy(APawn* Targeter, AUTPlayerState* PS)
{
	AUTCharacter* TargeterChar = Cast<AUTCharacter>(Targeter);
	if (TargeterChar)
	{
		TargeterChar->LastTarget = this;
		TargeterChar->LastTargetingTime = GetWorld()->GetTimeSeconds();
		TargeterChar->bHaveTargetVisual = true;
	}
	if ((LastTargetedTime == GetWorld()->GetTimeSeconds()) && (PS == LastTargeter))
	{
		// skip
		return;
	}
	LastTargetedTime = GetWorld()->GetTimeSeconds();
	LastTargetSeenTime = GetWorld()->GetTimeSeconds();
	LastTargeter = PS;

	AUTCarriedObject* Flag = GetCarriedObject();
	if (Flag && Flag->bShouldPingFlag)
	{
		if (PS && (GetWorld()->GetTimeSeconds() - Flag->LastPingVerbalTime > 12.f) && !Flag->bCurrentlyPinged)
		{
			Flag->LastPingVerbalTime = GetWorld()->GetTimeSeconds();
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS)
			{
				GS->LastEnemyLocationReportTime = GetWorld()->GetTimeSeconds();
			}
			AUTGameVolume* GV = UTCharacterMovement ? Cast<AUTGameVolume>(UTCharacterMovement->GetPhysicsVolume()) : nullptr;
			if (GV && (GV->VoiceLinesSet != NAME_None))
			{
				PS->AnnounceStatus(GV->VoiceLinesSet, 0);
				GS->LastEnemyLocationName = GV->VoiceLinesSet;
				AUTFlagRunGameState* FlagrunGS = Cast<AUTFlagRunGameState>(GS);
				if (FlagrunGS && FlagrunGS->CurrentRallyPoint)
				{
					FlagrunGS->CurrentRallyPoint->WarnEnemyRally();
				}
			}
			else
			{
				PS->AnnounceStatus(StatusMessage::EnemyFCHere);
			}
		}
		Flag->LastPinger = PS ? PS : Flag->LastPinger;
	}

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (TargeterChar && GS && GS->bPlayStatusAnnouncements && Cast<AUTPlayerController>(GetController()))
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerState);
		if (UTPlayerState && UTPlayerState->Team && (GetWorld()->GetTimeSeconds() - UTPlayerState->LastBehindYouTime > 12.f))
		{
			// announce behind you if attacker is behind this player && teammate can see it
			FVector ViewDir = GetActorRotation().Vector();
			FVector EnemyDir = (Targeter->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			if ((ViewDir | EnemyDir) < 0.5f)
			{
				// if teammate nearby, have them announce - send replicated function so client can verify last rendered time
				for (FConstControllerIterator Iterator = GetWorld()->GetControllerIterator(); Iterator; ++Iterator)
				{
					AController* C = *Iterator;
					AUTPlayerState* TeamPS = C ? Cast<AUTPlayerState>(C->PlayerState) : nullptr;
					if (TeamPS && (TeamPS != UTPlayerState) && C->GetPawn() && GS->OnSameTeam(this, C) && C->LineOfSightTo(this))
					{
						Cast<AUTPlayerController>(GetController())->ClientWarnEnemyBehind(TeamPS, TargeterChar);
						UTPlayerState->LastBehindYouTime = GetWorld()->GetTimeSeconds();
						break;
					}
				}
			}
		}
	}
}

void AUTCharacter::NotifyTakeHit(AController* InstigatedBy, int32 AppliedDamage, int32 Damage, FVector Momentum, AUTInventory* HitArmor, const FDamageEvent& DamageEvent)
{
	if (Role == ROLE_Authority)
	{
		AUTCarriedObject* Flag = GetCarriedObject();
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		AUTPlayerController* InstigatedByPC = Cast<AUTPlayerController>(InstigatedBy);
		APawn* InstigatorPawn = InstigatedBy ? InstigatedBy->GetPawn() : nullptr;
		uint8 CompressedDamage = FMath::Clamp(AppliedDamage, 0, 255);
		if (InstigatedByPC != NULL)
		{
			InstigatedByPC->ClientNotifyCausedHit(this, CompressedDamage);
		}
		else
		{
			AUTBot* InstigatedByBot = Cast<AUTBot>(InstigatedBy);
			if (InstigatedByBot != NULL)
			{
				InstigatedByBot->NotifyCausedHit(this, Damage);
			}
		}
		if (InstigatorPawn && GS && !GS->OnSameTeam(this, InstigatedBy))
		{
			TargetedBy(InstigatorPawn, Cast<AUTPlayerState>(InstigatedBy->PlayerState));
		}

		// we do the sound here instead of via PlayTakeHitEffects() so it uses RPCs instead of variable replication which is higher priority
		// (at small bandwidth cost)
		if (((GetController() == InstigatedBy) || !GS || !GS->OnSameTeam(this, InstigatedBy)) && (GetWorld()->TimeSeconds - LastPainSoundTime >= MinPainSoundInterval))
		{
			const UDamageType* const DamageTypeCDO = DamageEvent.DamageTypeClass ? DamageEvent.DamageTypeClass->GetDefaultObject<UDamageType>() : GetDefault<UDamageType>();
			const UUTDamageType* const UTDamageTypeCDO = Cast<UUTDamageType>(DamageTypeCDO); // warning: may be NULL
			if (HitArmor != nullptr && ((UTDamageTypeCDO == NULL) || UTDamageTypeCDO->bBlockedByArmor))
			{
				USoundBase* ArmorSound = (GetArmorAmount()+Damage/2 > 100) && HitArmor->ShieldDamageSound ? HitArmor->ShieldDamageSound : HitArmor->ReceivedDamageSound;
				if (ArmorSound != nullptr)
				{
					UUTGameplayStatics::UTPlaySound(GetWorld(), ArmorSound, this, SRT_All, false, FVector::ZeroVector, InstigatedByPC, NULL, false, SAT_PainSound);
				}
			}
			else if ((UTDamageTypeCDO == NULL) || UTDamageTypeCDO->bCausesPainSound)
			{
				UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->PainSound, this, SRT_All, false, FVector::ZeroVector, InstigatedByPC, NULL, false, SAT_PainSound);
			}
			LastPainSoundTime = GetWorld()->TimeSeconds;
		}

		if (InstigatorPawn)
		{
			// notify spectators watching this player
			for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
			{
				AUTPlayerController* PC = Cast<AUTPlayerController>(*It);
				if (PC != NULL && (PC != InstigatedByPC) && PC->GetViewTarget() == InstigatorPawn && PC->GetPawn() != this)
				{
					PC->ClientNotifyCausedHit(this, CompressedDamage);
				}
				else if (Cast<AUTDemoRecSpectator>(PC))
				{
					((AUTDemoRecSpectator*)(PC))->DemoNotifyCausedHit(InstigatorPawn, this, CompressedDamage, Momentum, DamageEvent);
				}
			}
		}

		AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
		if (PC != NULL)
		{
			// pass some damage even if armor absorbed all of it, so client will get visual hit indicator
			PC->NotifyTakeHit(InstigatedBy, HitArmor ? FMath::Max(Damage, 1) : Damage, Momentum, DamageEvent);
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

void AUTCharacter::OnRepFloorSliding()
{
	if (UTCharacterMovement)
	{
		UTCharacterMovement->bIsFloorSliding = bRepFloorSliding;
		// replay sim breaks if the capsule doesn't get updated correctly but SimulatedTick() doesn't do that for slide, force it here
		if (Role < ROLE_AutonomousProxy && UTCharacterMovement->NetworkSmoothingMode == ENetworkSmoothingMode::Replay)
		{
			if (bRepFloorSliding)
			{
				UTCharacterMovement->SetMovementMode(MOVE_Walking);
				UTCharacterMovement->Crouch(true);
			}
			else
			{
				UTCharacterMovement->UnCrouch(true);
			}
		}
	}
}

bool AUTCharacter::K2_Died(AController* EventInstigator, TSubclassOf<UDamageType> DamageType)
{
	return Died(EventInstigator, FUTPointDamageEvent(Health + 1, FHitResult(), FVector(0.0f, 0.0f, -1.0f), DamageType));
}

bool AUTCharacter::Died(AController* EventInstigator, const FDamageEvent& DamageEvent, AActor* DamageCauser)
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

		FHitResult HitInfo;
		{
			FVector UnusedDir;
			DamageEvent.GetBestHitInfo(this, NULL, HitInfo, UnusedDir);
		}

		// gameinfo hook to prevent deaths
		// WARNING - don't prevent bot suicides - they suicide when really needed
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (Game != NULL && Game->PreventDeath(this, EventInstigator, DamageEvent.DamageTypeClass, HitInfo))
		{
			Health = FMath::Max<int32>(Health, 1);
			return false;
		}
		else
		{
			bTearOff = true; // important to set this as early as possible so IsDead() returns true
			Health = FMath::Min<int32>(Health, 0);

			AUTRemoteRedeemer* Redeemer = Cast<AUTRemoteRedeemer>(DrivenVehicle);
			if (Redeemer != nullptr)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(Redeemer->PlayerState);
				if (PS != NULL && PS->CarriedObject != NULL)
				{
					PS->CarriedObject->Drop(EventInstigator);
				}
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
			if ((GetWorld()->GetTimeSeconds() - FlakShredTime < 0.05f) && FlakShredInstigator && (FlakShredInstigator == EventInstigator))
			{
				AnnounceShred(Cast<AUTPlayerController>(EventInstigator));
			}

			GetWorld()->GetAuthGameMode<AUTGameMode>()->Killed(EventInstigator, ControllerKilled, this, DamageEvent.DamageTypeClass);

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

			//Stop ghosts on death
			if (GhostComponent->bGhostRecording)
			{
				GhostComponent->GhostStopRecording();
			}
			if (GhostComponent->bGhostPlaying)
			{
				GhostComponent->GhostStopPlaying();
			}

			// Tell the client to play the killcam
			AUTPlayerController* DyingGameController = Cast<AUTPlayerController>(ControllerKilled);
			if (DyingGameController)
			{
				APawn* KillcamFocus = EventInstigator ? EventInstigator->GetPawn() : nullptr;
				FVector FocusLoc = Cast<AUTProjectile>(DamageCauser) ? ((AUTProjectile *)DamageCauser)->ShooterLocation : (KillcamFocus ? KillcamFocus->GetActorLocation() : FVector::ZeroVector);
				DyingGameController->ClientPlayKillcam(EventInstigator, KillcamFocus, FocusLoc);
			}

			return true;
		}
	}
}

void AUTCharacter::AnnounceShred(AUTPlayerController *KillerPC)
{
	AUTPlayerState* KillerPS = KillerPC ? Cast<AUTPlayerState>(KillerPC->PlayerState) : NULL;
	if (KillerPS && CloseFlakRewardMessageClass)
	{
		KillerPS->ModifyStatsValue(FlakShredStatName, 1);
		KillerPS->AddCoolFactorMinorEvent();
		KillerPS->bAnnounceWeaponReward = true;
		KillerPC->SendPersonalMessage(CloseFlakRewardMessageClass, KillerPS->GetStatsValue(FlakShredStatName), PlayerState, KillerPS);

	}
}

void AUTCharacter::StartRagdoll()
{
	if (IsActorBeingDestroyed() || !GetMesh()->IsRegistered())
	{
		return;
	}

	// force standing
	UTCharacterMovement->UnCrouch(true);
	if (RootComponent == GetMesh() && GetMesh()->IsSimulatingPhysics())
	{
		// UnCrouch() caused death
		return;
	}

	// Prevent re-entry
	if (!bStartingRagdoll)
	{
		// Prevent re-entry
		TGuardValue<bool> RagdollGuard(bStartingRagdoll, true);

		// turn off any taccom when dead
		if (bTearOff || !bFeigningDeath)
		{
			SetOutlineLocal(false);
		}

		SetActorEnableCollision(true);
		StopFiring();
		DisallowWeaponFiring(true);
		bInRagdollRecovery = false;

		if (!GetMesh()->ShouldTickPose())
		{
			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (AnimInstance == nullptr || !AnimInstance->IsPostUpdatingAnimation())
			{
				GetMesh()->TickAnimation(0.0f, false);
				GetMesh()->RefreshBoneTransforms();
				GetMesh()->UpdateComponentToWorld();
			}
		}
		GetCharacterMovement()->ApplyAccumulatedForces(0.0f);
		// enable CCD on root body to avoid passing through surfaces at high velocity/low framerate (let the other bones clip for perf)
		if (GetMesh()->GetBodyInstance() != nullptr)
		{
			GetMesh()->GetBodyInstance()->bUseCCD = true;
		}
		GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetMesh()->SetAllBodiesNotifyRigidBodyCollision(true); // note that both the component and the body instance need this set for it to apply
		GetMesh()->UpdateKinematicBonesToAnim(GetMesh()->GetComponentSpaceTransforms(), ETeleportType::TeleportPhysics, true);
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->RefreshBoneTransforms();
		GetMesh()->SetAllBodiesPhysicsBlendWeight(1.0f);
		GetMesh()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		RootComponent = GetMesh();
		GetMesh()->bGenerateOverlapEvents = true;
		GetMesh()->bShouldUpdatePhysicsVolume = true;
		GetMesh()->RegisterClothTick(true);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetCapsuleComponent()->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		GetCapsuleComponent()->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepWorldTransform);

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
		bApplyWallSlide = false;

		// set up the custom physics override, if necessary
		SetRagdollGravityScale(RagdollGravityScale);

		TopRagdollSpeed = 0.0f;
	}
}

void AUTCharacter::StopRagdoll()
{
	// check for falling damage
	if (!IsDead())
	{
		CheckRagdollFallingDamage(FHitResult(NULL, NULL, GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)));
	}

	UTCharacterMovement->Velocity = GetMesh()->GetComponentVelocity();
	UTCharacterMovement->PendingLaunchVelocity = FVector::ZeroVector;

	GetCapsuleComponent()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	FRotator FixedRotation = GetCapsuleComponent()->RelativeRotation;
	FixedRotation.Pitch = FixedRotation.Roll = 0.0f;
	if (Controller != NULL)
	{
		// always recover in the direction the controller is facing since turning is instant
		FixedRotation.Yaw = Controller->GetControlRotation().Yaw;
	}
	GetCapsuleComponent()->SetRelativeRotation(FixedRotation);
	GetCapsuleComponent()->SetRelativeScale3D(GetClass()->GetDefaultObject<AUTCharacter>()->GetCapsuleComponent()->RelativeScale3D);
	if ((Role == ROLE_Authority) || IsLocallyControlled())
	{
		GetCapsuleComponent()->SetCapsuleSize(GetCapsuleComponent()->GetUnscaledCapsuleRadius(), GetCharacterMovement()->CrouchedHalfHeight);
		bIsCrouched = true;
	}
	RootComponent = GetCapsuleComponent();

	GetMesh()->MeshComponentUpdateFlag = GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->MeshComponentUpdateFlag;
	GetMesh()->bBlendPhysics = false; // for some reason bBlendPhysics == false is the value that actually blends instead of using only physics
	GetMesh()->bGenerateOverlapEvents = false;
	GetMesh()->bShouldUpdatePhysicsVolume = false;
	GetMesh()->RegisterClothTick(false);

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
			TArray<FTransform> BodyTransforms;
			for (int32 i = 0; i < GetMesh()->Bodies.Num(); i++)
			{
				BodyTransforms.Add((GetMesh()->Bodies[i] != NULL) ? GetMesh()->Bodies[i]->GetUnrealWorldTransform() : FTransform::Identity);
			}

			const USkeletalMeshComponent* DefaultMesh = GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh();
			FTransform RelativeTransform(DefaultMesh->RelativeRotation, DefaultMesh->RelativeLocation, DefaultMesh->RelativeScale3D);
			GetMesh()->SetWorldTransform(RelativeTransform * GetCapsuleComponent()->GetComponentTransform(), false, nullptr, ETeleportType::TeleportPhysics);

			RootBody->SetBodyTransform(GetMesh()->GetComponentTransform(), ETeleportType::TeleportPhysics);
			RootBody->PutInstanceToSleep();
			RootBody->SetInstanceSimulatePhysics(false, true);
			RootBody->PhysicsBlendWeight = 1.0f; // second parameter of SetInstanceSimulatePhysics() doesn't actually work at the moment...
			for (int32 i = 0; i < GetMesh()->Bodies.Num(); i++)
			{
				if (GetMesh()->Bodies[i] != NULL && GetMesh()->Bodies[i] != RootBody)
				{
					GetMesh()->Bodies[i]->SetBodyTransform(BodyTransforms[i], ETeleportType::TeleportPhysics);
					GetMesh()->Bodies[i]->PutInstanceToSleep();
					//GetMesh()->Bodies[i]->SetInstanceSimulatePhysics(false, true);
					//GetMesh()->Bodies[i]->PhysicsBlendWeight = 1.0f;
				}
			}
			//GetMesh()->SyncComponentToRBPhysics();
		}
	}

	bInRagdollRecovery = true;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AUTCharacter::SetRagdollGravityScale(float NewScale)
{
	for (FBodyInstance* Body : GetMesh()->Bodies)
	{
		if (Body != NULL)
		{
			Body->SetEnableGravity(NewScale != 0.0f);
		}
	}
	RagdollGravityScale = NewScale;
}

FVector AUTCharacter::GetLocationCenterOffset() const
{
	return (!IsRagdoll() || RootComponent != GetMesh()) ? FVector::ZeroVector : (GetMesh()->Bounds.Origin - GetMesh()->GetComponentLocation());
}

bool AUTCharacter::IsRecentlyDead()
{
	return IsDead() && (GetWorld()->GetTimeSeconds() - TimeOfDeath < 1.f);
}

void AUTCharacter::PlayDeathSound()
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->DeathSound, this, SRT_None, false, FVector::ZeroVector, NULL, NULL, false, SAT_PainSound);
}

void AUTCharacter::PlayDying()
{
	TimeOfDeath = GetWorld()->TimeSeconds;

	SetOutlineLocal(false);
	SpawnBloodDecal(GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), FVector(0.0f, 0.0f, -1.0f));
	LastDeathDecalTime = GetWorld()->TimeSeconds;

	// Set the hair back to normal because hats are being removed
	if (GetMesh())
	{
		GetMesh()->SetMorphTarget(FName(TEXT("HatHair")), 0.0f);
	}

	if (LeaderHat && LeaderHat->GetAttachParentActor())
	{
		LeaderHat->OnWearerDeath(LastTakeHitInfo.DamageType);
	}
	else if (Hat && Hat->GetAttachParentActor())
	{
		Hat->OnWearerDeath(LastTakeHitInfo.DamageType);
	}

	if (Eyewear && Eyewear->GetAttachParentActor())
	{
		Eyewear->OnWearerDeath(LastTakeHitInfo.DamageType);
	}

	if (GetNetMode() != NM_DedicatedServer && !IsPendingKillPending() && (GetWorld()->TimeSeconds - GetLastRenderTime() < 3.0f || IsLocallyViewed()))
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
				bool bPlayedDeathAnim = false;
				UAnimMontage* DeathAnim = (UTDmg != NULL) ? UTDmg.GetDefaultObject()->GetDeathAnim(this) : NULL;
				if (DeathAnim != NULL && !IsInWater())
				{
					UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
					if (AnimInstance != NULL && AnimInstance->Montage_Play(DeathAnim))
					{
						bPlayedDeathAnim = true;
						FOnMontageBlendingOutStarted EndDelegate;
						EndDelegate.BindUObject(this, &AUTCharacter::DeathAnimEnd);
						AnimInstance->Montage_SetBlendingOutDelegate(EndDelegate);
					}
				}
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS && GS->IsMatchIntermission() && !IsLocallyViewed())
				{
					Destroy();
				}
				else if (bPlayedDeathAnim)
				{
					GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
					GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
					GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_TRANSDISK, ECR_Ignore);
				}
				else
				{
					StartRagdoll();
				}
			}
			if (UTDmg != NULL)
			{
				UTDmg.GetDefaultObject()->PlayDeathEffects(this);
			}
			// StartRagdoll() changes collision properties, which can potentially result in a new overlap -> more damage -> gib explosion -> Destroy()
			// SetTimer() has a dumb assert if the target of the function is already destroyed, so we need to check it ourselves
			if (!IsPendingKillPending())
			{
				if (!UTDmg || !UTDmg.GetDefaultObject()->OverrideDeathSound(this))
				{
					GetWorldTimerManager().SetTimer(DeathSoundHandle, this, &AUTCharacter::PlayDeathSound, 0.25f, false);
				}
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

	if (Hat && Hat->GetAttachParentActor())
	{
		Hat->DetachRootComponentFromParent(true);

		if (Hat->bDontDropOnDeath)
		{
			Hat->Destroy();
		}
		else
		{
			Hat->SetLifeSpan(7.0f);
		}
	}

	if (LeaderHat && LeaderHat->GetAttachParentActor())
	{
		LeaderHat->DetachRootComponentFromParent(true);

		if (LeaderHat->bDontDropOnDeath)
		{
			LeaderHat->Destroy();
		}
		else
		{
			LeaderHat->SetLifeSpan(7.0f);
		}
	}

	if (Eyewear && Eyewear->GetAttachParentActor())
	{
		Eyewear->DetachRootComponentFromParent(true);
	}
}

void AUTCharacter::DeathAnimEnd(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bHidden && !IsRagdoll())
	{
		StartRagdoll();
	}
}

void AUTCharacter::GibExplosion_Implementation()
{
	if (bAllowGibs && CharacterData != NULL)
	{
		const AUTCharacterContent* CharDataObj = CharacterData.GetDefaultObject();
		if (CharDataObj->GibExplosionEffect != NULL)
		{
			CharDataObj->GibExplosionEffect.GetDefaultObject()->SpawnEffect(GetWorld(), RootComponent->GetComponentTransform(), GetMesh(), this, NULL, SRT_None);
		}
		for (const FGibSlotInfo& GibSlot : CharDataObj->Gibs)
		{
			SpawnGib(GibSlot, *LastTakeHitInfo.DamageType);
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

void AUTCharacter::SpawnGib(const FGibSlotInfo& GibInfo, TSubclassOf<UUTDamageType> DmgType)
{
	if (GibInfo.GibType != NULL && bAllowGibs)
	{
		TSubclassOf<AUTGib> GibClass = GibInfo.GibType;
		
		TSubclassOf<AUTGib> HatGibClass = nullptr;
		if (LeaderHat)
		{
			HatGibClass = LeaderHat->OverrideGib(GibInfo.BoneName);
		}
		else if (Hat)
		{
			HatGibClass = Hat->OverrideGib(GibInfo.BoneName);
		}
		if (HatGibClass)
		{
			GibClass = HatGibClass;
		}

		FTransform SpawnPos = GetMesh()->GetSocketTransform(GibInfo.BoneName);
		if (SpawnPos.GetScale3D().Size() <= 0.0f)
		{
			// on client headshot scale-to-zero may have already been applied
			SpawnPos.RemoveScaling();
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
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
				FVector Vel = (GetMesh() == RootComponent) ? GetMesh()->GetComponentVelocity() : (UTCharacterMovement->Velocity + UTCharacterMovement->GetPendingImpulse());
				Vel += (Gib->GetActorLocation() - GetActorLocation()).GetSafeNormal() * FMath::Max<float>(400.0f, Vel.Size() * 0.25f);
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
	if (GetWorld()->TimeSeconds - CreationTime > 1.0f) // workaround for feign death immediately on spawn bug
	{
		ServerFeignDeath();
	}
}
bool AUTCharacter::ServerFeignDeath_Validate()
{
	return true;
}
void AUTCharacter::ServerFeignDeath_Implementation()
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (Role == ROLE_Authority && 
		!IsDead() && 
		(!GS || (GS->IsMatchInProgress() && !GS->IsMatchIntermission()) || (GS->GetMatchState() == MatchState::WaitingToStart && PS && PS->bIsWarmingUp)) &&
		!BlockFeignDeath())
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
				// don't allow unfeigning if flying through the air
				if (IsInWater() || GetWorld()->SweepTestByChannel(ActorLocation + TraceOffset, ActorLocation - TraceOffset, FQuat::Identity, ECC_Pawn, CapsuleShape, FeignDeathTrace))
				{
					// Expand in place 
					TArray<FOverlapResult> Overlaps;
					bool bEncroached = GetWorld()->OverlapMultiByChannel(Overlaps, ActorLocation, FQuat::Identity, ECC_Pawn, CapsuleShape, CapsuleParams);
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
						UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->PainSound, this, SRT_All, false, FVector::ZeroVector, NULL, NULL, false, SAT_PainSound);
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
		}
		else if (GetMesh()->Bodies.Num() == 0 || GetMesh()->Bodies[0]->PhysicsBlendWeight <= 0.0f)
		{
			if (PS != nullptr && GetWorld()->GetTimerManager().IsTimerActive(PS->RallyTimerHandle))
			{
				// no feigning while in rally
				return;
			}
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
		UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->DeathSound, this, SRT_None, false, FVector::ZeroVector, NULL, NULL, false, SAT_PainSound);
		if (Role == ROLE_Authority)
		{
			DropFlag();
		}

		if (TauntCount > 0)
		{
			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Stop(0.0f);
			}
		}
		if (Weapon != nullptr && Weapon->DroppedPickupClass != nullptr && Weapon->bCanThrowWeapon)
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

	SetAmbientSound(NULL);
	SetLocalAmbientSound(NULL);
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
			AmbientSoundComp->SetupAttachment(RootComponent);
			AmbientSoundComp->RegisterComponent();
		}
		if (AmbientSoundComp->Sound != AmbientSound)
		{
			// don't attenuate/spatialize sounds made by a local viewtarget
			AmbientSoundComp->bAllowSpatialization = true;
			AmbientSoundComp->SetPitchMultiplier(1.f);

			if (GEngine->GetMainAudioDevice() && !GEngine->GetMainAudioDevice()->IsHRTFEnabledForAll())
			{
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					if (It->PlayerController != NULL && It->PlayerController->GetViewTarget() == this)
					{
						AmbientSoundComp->bAllowSpatialization = false;
						break;
					}
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

void AUTCharacter::ChangeAmbientSoundPitch(USoundBase* InAmbientSound, float NewPitch)
{
	if (AmbientSoundComp && AmbientSound && (AmbientSound == InAmbientSound))
	{
		AmbientSoundPitch = NewPitch;
		AmbientSoundPitchUpdated();
	}
}

void AUTCharacter::AmbientSoundPitchUpdated()
{
	if (AmbientSoundComp && AmbientSound)
	{
		AmbientSoundComp->SetPitchMultiplier(AmbientSoundPitch);
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
			LocalAmbientSoundComp->SetupAttachment(RootComponent);
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
			StatusAmbientSoundComp->SetupAttachment(RootComponent);
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
	else if (Weapon != NULL && TauntCount == 0)
	{
		Weapon->StartFire(FireModeNum);
	}

	if (GhostComponent->bGhostRecording && !IsFiringDisabled())
	{
		GhostComponent->GhostStartFire(FireModeNum);
	}
}

void AUTCharacter::StopFire(uint8 FireModeNum)
{
	if (DrivenVehicle ? !DrivenVehicle->IsLocallyControlled() : !IsLocallyControlled())
	{
			// UE_LOG(LogUTCharacter, Warning, TEXT("StopFire() can only be called on the owning client"));
	}
	else if (Weapon != NULL)
	{
		Weapon->StopFire(FireModeNum);
	}
	else
	{
		SetPendingFire(FireModeNum, false);
	}

	if (GhostComponent->bGhostRecording && !IsFiringDisabled())
	{
		GhostComponent->GhostStopFire(FireModeNum);
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

bool AUTCharacter::IsTriggerDown(uint8 InFireMode)
{
	return IsPendingFire(InFireMode);
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
	if ((FlashCount & 0xF) == 0)
	{
		FlashCount++;
	}
	FireMode = InFireMode;

	//Pack the Firemode in top 4 bits to prevent misfires when alternating projectile shots
	//eg pri shot, FC = 1 -> alt shot, FC = 0 (stop fire) -> FC = 1  (FC is still 1 so no rep)
	FlashCount = (FlashCount & 0x0F) | FireMode << 4;
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
	if (GetWorld()->DemoNetDriver && GetWorld()->DemoNetDriver->IsFastForwarding())
	{
		return;
	}

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

	AUTPlayerController* UTPC = GetLocalViewer();
	if ((bLocalFlashLoc || UTPC == NULL || UTPC->GetPredictionTime() == 0.f || !IsLocallyControlled()) && Weapon != NULL && Weapon->ShouldPlay1PVisuals())
	{
		if (!FlashLocation.IsZero())
		{
			uint8 EffectFiringMode = Weapon->GetCurrentFireMode();
			// if non-local first person spectator, also play firing effects from here
			if (Controller == NULL)
			{
				EffectFiringMode = FireMode;
				Weapon->FiringInfoUpdated(FireMode, FlashCount, FlashLocation);
				Weapon->FiringEffectsUpdated(FireMode, FlashLocation);
			}
			else
			{
				FVector SpawnLocation;
				FRotator SpawnRotation;
				Weapon->GetImpactSpawnPosition(FlashLocation, SpawnLocation, SpawnRotation);
				Weapon->PlayImpactEffects(FlashLocation, EffectFiringMode, SpawnLocation, SpawnRotation);
			}
		}
		else if (Controller == NULL)
		{
			Weapon->FiringInfoUpdated(FireMode, FlashCount, FlashLocation);
		}
		if (FlashCount == 0 && FlashLocation.IsZero() && WeaponAttachment != NULL)
		{
			WeaponAttachment->StopFiringEffects();
		}
	}
	else if (WeaponAttachment != NULL)
	{
		if (FlashCount != 0 || !FlashLocation.IsZero())
		{
			if ((!IsLocallyControlled() || UTPC == NULL || UTPC->IsBehindView()))
			{
				WeaponAttachment->PlayFiringEffects();
			}
		}
		else
		{
			// always call Stop to avoid effects mismatches where we switched view modes during a firing sequence
			// and some effect ends up being left on forever
			WeaponAttachment->StopFiringEffects();
		}
	}
}
void AUTCharacter::FiringExtraUpdated()
{
	if (GetWorld()->DemoNetDriver && GetWorld()->DemoNetDriver->IsFastForwarding())
	{
		return;
	}

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(Controller);
	if (WeaponAttachment != NULL && (!IsLocallyControlled() || UTPC == NULL || UTPC->IsBehindView()))
	{
		WeaponAttachment->FiringExtraUpdated();
	}
	if (Weapon != nullptr && UTPC == nullptr && Role < ROLE_Authority)
	{
		Weapon->FiringExtraUpdated(FlashExtra, FireMode);
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

void AUTCharacter::RestoreAmmoPct(float Pct, bool bPctOfMax)
{
	for (TInventoryIterator<AUTWeapon> It(this); It; ++It)
	{
		It->AddAmmo(Pct * (bPctOfMax ? It->MaxAmmo : It->Ammo));
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
	if ((GetNetMode() == NM_Standalone) || (GetNetMode() == NM_DedicatedServer))
	{
		for (TInventoryIterator<AUTWeapon> It(this); It; ++It)
		{
			if (!It->bMustBeHolstered)
			{
				It->AddAmmo(It->MaxAmmo);
			}
		}
	}
}

AUTInventory* AUTCharacter::K2_CreateInventory(TSubclassOf<AUTInventory> NewInvClass, bool bAutoActivate)
{
	AUTInventory* Inv = NULL;
	if (NewInvClass != NULL)
	{
		Inv = GetWorld()->SpawnActor<AUTInventory>(NewInvClass);
		if (Inv != NULL)
		{
			if (!AddInventory(Inv, bAutoActivate))
			{
				Inv->Destroy();
				Inv = NULL;
			}
		}
	}

	return Inv;
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

bool AUTCharacter::AddInventory(AUTInventory* InvToAdd, bool bAutoActivate)
{
	if (InvToAdd != NULL && !InvToAdd->IsPendingKillPending())
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
			else
			{
				AUTInventory* Last = InventoryList;
				for (AUTInventory* Item = InventoryList; Item != NULL; Item = Item->NextInventory)
				{
					if (Item == InvToAdd)
					{
						UE_LOG(UT, Warning, TEXT("AddInventory: %s already in %s's inventory!"), *InvToAdd->GetName(), *GetName());
						return false;
					}
					Last = Item;
				}
				Last->NextInventory = InvToAdd;
			}
			InvToAdd->GivenTo(this, bAutoActivate);
			
			if (InvToAdd->GetOwner() == this)
			{
				AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
				if (Game != NULL && Game->BaseMutator != NULL)
				{
					Game->BaseMutator->ModifyInventory(InvToAdd, this);
				}
			}

			InvToAdd->UpdateHUDText();
			return true;
		}
	}

	return false;
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
			// this can happen during replays
			//UE_LOG(UT, Warning, TEXT("RemoveInventory (%s): Item %s was not in this character's inventory!"), *GetName(), *InvToRemove->GetName());
		}
		else
		{
			if (InvToRemove == PendingWeapon)
			{
				SetPendingWeapon(NULL);
			}
			else if (InvToRemove == Weapon)
			{
				Weapon = NULL;
				if (PendingWeapon != NULL)
				{
					WeaponChanged();
				}
				// note: this function is called for spectators when switching viewed player but we need to make sure not to mess with replicated properties for non-owners
				else if (Role == ROLE_Authority || IsLocallyControlled())
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
	if (Role == ROLE_Authority)
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
	if (NewWeapon != NULL && !IsDead() && ((Weapon == nullptr) || !IsFiringDisabled()))
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
			if (Role < ROLE_Authority)
			{
				ServerSwitchWeapon(NewWeapon);
			}
			LocalSwitchWeapon(NewWeapon);
		}
	}
}

void AUTCharacter::SetPendingWeapon(AUTWeapon* NewPendingWeapon)
{
	PendingWeapon = NewPendingWeapon;
	bIsSwitchingWeapon = (PendingWeapon != NULL);
	if (bIsSwitchingWeapon && IsLocallyControlled() && Cast<APlayerController>(GetController()))
	{
		((APlayerController*)(GetController()))->ClientReceiveLocalizedMessage(UUTPickupMessage::StaticClass(), 0, NULL, NULL, PendingWeapon->GetClass());
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
					SetPendingWeapon(NewWeapon);
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
						SetPendingWeapon(NewWeapon);
					}
				}
				else if (PendingWeapon != NULL)
				{
					// switching back to weapon that was on its way down
					SetPendingWeapon(NULL);
					Weapon->BringUp();
				}
			}
			else if (Weapon != NULL && PendingWeapon != NULL && PendingWeapon->PutDown())
			{
				// stopping weapon switch in progress by passing NULL
				SetPendingWeapon(NULL);
				Weapon->BringUp();
			}
		}
	}
}

void AUTCharacter::ClientSwitchWeapon_Implementation(AUTWeapon* NewWeapon)
{
	if (Role < ROLE_Authority)
	{
		ServerSwitchWeapon(NewWeapon);
	}
	LocalSwitchWeapon(NewWeapon);
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

void AUTCharacter::ServerVerifyWeapon_Implementation(AUTWeapon* NewWeapon)
{
	if (Role == ROLE_Authority && !IsLocallyControlled() && NewWeapon != nullptr && Weapon != NewWeapon && PendingWeapon != NewWeapon)
	{
		UE_LOG(UT, Warning, TEXT("%s (%s) weapon mismatch: server %s, client %s"), *GetName(), (PlayerState != nullptr) ? *PlayerState->PlayerName : TEXT("None"), *GetNameSafe((PendingWeapon != nullptr) ? PendingWeapon : Weapon), *GetNameSafe(NewWeapon));
		LocalSwitchWeapon(NewWeapon);
		if (Weapon != NewWeapon && PendingWeapon != NewWeapon)
		{
			UE_LOG(UT, Warning, TEXT(" - weapon client wanted was invalid"));
			ClientSwitchWeapon((PendingWeapon != nullptr) ? PendingWeapon : Weapon);
		}
	}
}
bool AUTCharacter::ServerVerifyWeapon_Validate(AUTWeapon* NewWeapon)
{
	return true;
}

void AUTCharacter::ClientVerifyWeapon_Implementation()
{
	// if there is a pending weapon then the verification will happen when WeaponChanged() is called
	if (PendingWeapon == NULL)
	{
		ServerVerifyWeapon(Weapon);
	}
}

void AUTCharacter::WeaponChanged(float OverflowTime)
{
	if (PendingWeapon != NULL && PendingWeapon->GetUTOwner() == this)
	{
		checkSlow(IsInInventory(PendingWeapon));
		Weapon = PendingWeapon;
		SetPendingWeapon(NULL);
		WeaponClass = Weapon->GetClass();
		WeaponAttachmentClass = Weapon->AttachmentType;
		Weapon->BringUp(OverflowTime);
		UpdateWeaponSkinPrefFromProfile(Weapon);
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

	if (Role < ROLE_Authority)
	{
		ServerVerifyWeapon(Weapon);
	}

	if (GhostComponent->bGhostRecording && Weapon != nullptr)
	{
		GhostComponent->GhostSwitchWeapon(Weapon);
	}

	// Reset the speed modifier if we need to.
	if (Role == ROLE_Authority)
	{
		ResetMaxSpeedPctModifier();
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
			SetPendingWeapon(NULL);
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

void AUTCharacter::SetSkinForWeapon(UUTWeaponSkin* WeaponSkin)
{
	bool bAlreadyAssigned = false;
	for (int32 i = 0; i < WeaponSkins.Num(); i++)
	{
		if (WeaponSkins[i]->WeaponType == WeaponSkin->WeaponType)
		{
			WeaponSkins[i] = WeaponSkin;
			bAlreadyAssigned = true;
		}
	}

	if (!bAlreadyAssigned)
	{
		WeaponSkins.Add(WeaponSkin);
	}
}

void AUTCharacter::UpdateWeaponSkinPrefFromProfile(AUTWeapon* InWeapon)
{
	if (InWeapon != nullptr && IsLocallyControlled())
	{
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		if (PS)
		{
			PS->UpdateWeaponSkinPrefFromProfile(InWeapon);
		}
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
			if (WeaponAttachment != NULL)
			{
				WeaponAttachment->AttachToOwner();
			}
		}

		UpdateWeaponSkin();
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
			if (HolsteredWeaponAttachment != NULL)
			{
				HolsteredWeaponAttachment->HolsterToOwner();
			}
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
	DOREPLIFETIME_CONDITION(AUTCharacter, Weapon, COND_SkipOwner);

	DOREPLIFETIME_CONDITION(AUTCharacter, FlashCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashLocation, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashExtra, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, LastTakeHitInfo, COND_Custom);
	DOREPLIFETIME_CONDITION(AUTCharacter, MovementEvent, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponClass, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponAttachmentClass, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, bApplyWallSlide, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, bTriggerRallyEffect, COND_None);

	DOREPLIFETIME_CONDITION(AUTCharacter, HolsteredWeaponAttachmentClass, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, DamageScaling, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireRateMultiplier, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, AmbientSound, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, CharOverlayFlags, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponOverlayFlags, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, ReplicatedBodyMaterial, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, ReplicatedBodyMaterial1P, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, HeadScale, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, bFeigningDeath, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, bRepFloorSliding, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, bIsInCombat, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, bSpawnProtectionEligible, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, DrivenVehicle, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, WalkMovementReductionPct, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, WalkMovementReductionTime, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, bInvisible, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, HeadArmorFlashCount, COND_Custom);
	DOREPLIFETIME_CONDITION(AUTCharacter, bIsSwitchingWeapon, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, AmbientSoundPitch, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, CosmeticFlashCount, COND_Custom);
	DOREPLIFETIME_CONDITION(AUTCharacter, CosmeticSpreeCount, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, ArmorAmount, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, ArmorType, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, WeaponSkins, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, VisibilityMask, COND_None);
	DOREPLIFETIME_CONDITION(AUTCharacter, bCanRally, COND_OwnerOnly);

	DOREPLIFETIME_CONDITION(AUTCharacter, MaxSpeedPctModifier, COND_None);
	DOREPLIFETIME(AUTCharacter, bServerOutline);
	DOREPLIFETIME(AUTCharacter, bOutlineWhenUnoccluded);
	DOREPLIFETIME(AUTCharacter, ServerOutlineTeamMask);
}

static AUTWeapon* SavedWeapon = NULL;
static bool bSavedIsSwitchingWeapon = false;
void AUTCharacter::PreNetReceive()
{
	Super::PreNetReceive();

	SavedWeapon = Weapon;
	bSavedIsSwitchingWeapon = bIsSwitchingWeapon;
}
void AUTCharacter::PostNetReceive()
{
	Super::PostNetReceive();

	if (Weapon != SavedWeapon)
	{
		if (SavedWeapon != NULL && SavedWeapon->Mesh->IsAttachedTo(CharacterCameraComponent))
		{
			SavedWeapon->DetachFromOwner();
		}
	}
	if (Weapon != NULL && Weapon->GetUTOwner() == this && !Weapon->Mesh->IsAttachedTo(CharacterCameraComponent) && Weapon->ShouldPlay1PVisuals())
	{
		Weapon->AttachToOwner();
		// play weapon switch anim from here for 1P spectators
		if (Controller == NULL && (bIsSwitchingWeapon || bSavedIsSwitchingWeapon))
		{
			Weapon->PlayWeaponAnim(Weapon->BringUpAnim, Weapon->BringUpAnimHands);
		}
	}
	else if (Weapon != NULL && Controller == NULL && bIsSwitchingWeapon && !bSavedIsSwitchingWeapon)
	{
		Weapon->PlayWeaponAnim(Weapon->PutDownAnim, Weapon->PutDownAnimHands);
	}
}

void AUTCharacter::AddDefaultInventory(TArray<TSubclassOf<AUTInventory>> DefaultInventoryToAdd)
{
	// Check to see if this player has an active loadout.  If they do, apply it.  NOTE: Loadouts are 100% authoratative.  So if we apply any type of loadout, then end the AddDefaultInventory 
	// call right there.  If you are using the loadout system and want to insure a player has some default items, use bDefaultInclude and make sure their cost is 0.

	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerState);
	if (UTPlayerState)
	{
		if (UTPlayerState->PrimarySpawnInventory || UTPlayerState->SecondarySpawnInventory)
		{
			// Use the Spawn Inventory
			if (UTPlayerState->PrimarySpawnInventory)
			{
				AddInventory(GetWorld()->SpawnActor<AUTInventory>(UTPlayerState->PrimarySpawnInventory->ItemClass, FVector(0.0f), FRotator(0, 0, 0)), true);
			}

			// Use the Spawn Inventory
			if (UTPlayerState->SecondarySpawnInventory)
			{
				AddInventory(GetWorld()->SpawnActor<AUTInventory>(UTPlayerState->SecondarySpawnInventory->ItemClass, FVector(0.0f), FRotator(0, 0, 0)), true);
			}

			return;
		}

		if ( UTPlayerState->Loadout.Num() > 0 )
		{
			for (int32 i=0; i < UTPlayerState->Loadout.Num(); i++)
			{
				if (UTPlayerState->GetAvailableCurrency() >= UTPlayerState->Loadout[i]->CurrentCost)
				{
					AddInventory(GetWorld()->SpawnActor<AUTInventory>(UTPlayerState->Loadout[i]->ItemClass, FVector(0.0f), FRotator(0, 0, 0)), true);
					UTPlayerState->AdjustCurrency(UTPlayerState->Loadout[i]->CurrentCost * -1);
				}
			}

			return;
		}

		if (UTPlayerState->CurrentLoadoutPackTag != NAME_None)
		{
			// Verify it's valid.
			AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
			if ( UTGameMode )
			{
				int32 PackIndex = UTGameMode->LoadoutPackIsValid(UTPlayerState->CurrentLoadoutPackTag);
				if (PackIndex != INDEX_NONE && PackIndex < UTGameMode->AvailableLoadoutPacks.Num())
				{
					for (int32 i=0; i < UTGameMode->AvailableLoadoutPacks[PackIndex].LoadoutCache.Num(); i++)
					{
						AUTReplicatedLoadoutInfo* Info = UTGameMode->AvailableLoadoutPacks[PackIndex].LoadoutCache[i];
						AddInventory(GetWorld()->SpawnActor<AUTInventory>(Info->ItemClass, FVector(0.0f), FRotator(0, 0, 0)), true);
					}

					Health += UTGameMode->AvailableLoadoutPacks[PackIndex].SpawnHealthModifier;
					return;
				}
			}
		
		}

		if (UTPlayerState->PreservedKeepOnDeathInventoryList.Num() > 0)
		{
			for(AUTInventory* PreservedItem : UTPlayerState->PreservedKeepOnDeathInventoryList)
			{
				AddInventory(PreservedItem,false);
			}

			UTPlayerState->PreservedKeepOnDeathInventoryList.Empty();
		}
	}

	// Add the default character inventory
	for (int32 i=0;i<DefaultCharacterInventory.Num();i++)
	{
		AddInventory(GetWorld()->SpawnActor<AUTInventory>(DefaultCharacterInventory[i], FVector(0.0f), FRotator(0, 0, 0)), true);
	}

	// Add the default inventory passed in from the game
	for (int32 i=0;i<DefaultInventoryToAdd.Num();i++)
	{
		AddInventory(GetWorld()->SpawnActor<AUTInventory>(DefaultInventoryToAdd[i], FVector(0.0f), FRotator(0, 0, 0)), true);
	}
}

void AUTCharacter::SetInitialHealth_Implementation()
{
	Health = HealthMax;
	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerState);

	if (UTPlayerState && UTPlayerState->CurrentLoadoutPackTag != NAME_None)
	{
		// Verify it's valid.
		AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if ( UTGameMode )
		{
			int32 PackIndex = UTGameMode->LoadoutPackIsValid(UTPlayerState->CurrentLoadoutPackTag);
			if (PackIndex != INDEX_NONE && PackIndex < UTGameMode->AvailableLoadoutPacks.Num())
			{
				Health += UTGameMode->AvailableLoadoutPacks[PackIndex].SpawnHealthModifier;
			}
		}
	}
}

bool AUTCharacter::CanSlide() const
{
	return !bIsCrouched && UTCharacterMovement && !UTCharacterMovement->bIsFloorSliding && UTCharacterMovement->CanEverJump() && (UTCharacterMovement->Velocity.Z > -1.f * MaxSafeFallSpeed) && !IsThirdPersonTaunting() && !IsRagdoll() && !bInRagdollRecovery;
}

bool AUTCharacter::CanDodge() const
{
	return CanDodgeInternal();
}

bool AUTCharacter::CanDodgeInternal_Implementation() const
{
	return !bIsCrouched && UTCharacterMovement && UTCharacterMovement->CanDodge() && (UTCharacterMovement->Velocity.Z > -1.f * MaxSafeFallSpeed) && !IsThirdPersonTaunting() && !IsRagdoll() && !bInRagdollRecovery;
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
		bool bPotentialWallDodge = !UTCharacterMovement->IsMovingOnGround();

		if (UTCharacterMovement->PerformDodge(DodgeDir, DodgeCross))
		{
			MovementEventUpdated(bPotentialWallDodge ? EME_WallDodge : EME_Dodge, DodgeDir);
			InventoryEvent(InventoryEventName::Dodge);
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
	AUTPlayerController* PC = GetLocalViewer();
	return PC != NULL ? PC->PlayerCameraManager : NULL;
}

USoundBase* AUTCharacter::GetFootstepSoundForSurfaceType(EPhysicalSurface SurfaceType, bool bLocalPlayer)
{
	USoundBase** SoundPtr = nullptr;

	if (bLocalPlayer)
	{
		SoundPtr = OwnFootstepSoundsMap.Find(SurfaceType);
		return SoundPtr ? *SoundPtr : nullptr;
	}
	
	SoundPtr = FootstepSoundsMap.Find(SurfaceType);
	return SoundPtr ? *SoundPtr : nullptr;
}

void AUTCharacter::PlayFootstep(uint8 FootNum, bool bFirstPerson)
{
	if ((GetWorld()->TimeSeconds - LastFootstepTime < 0.1f) || bFeigningDeath || IsDead() || bIsCrouched || (GetNetMode() == NM_DedicatedServer))
	{
		return;
	}

	// Filter out the case where a local player is in a map with reflections so the third person mesh is rendered along with first person view bob
	// causing double footstep sounds to play. Just play the first person ones.
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(Controller);
	if (UTPC && IsLocallyControlled() && !bFirstPerson && !UTPC->IsBehindView())
	{
		return;
	}

	UParticleSystem* FootStepEffect = NULL;
	float MaxParticleDist = 1500.f;
	USoundBase* FootstepSoundToPlay = FootstepSound;
	if (FeetAreInWater())
	{
		FootstepSoundToPlay = WaterFootstepSound;
		FootStepEffect = WaterFootstepEffect;
		MaxParticleDist = 5000.f;
	}
	else
	{
		const bool bLocalViewer = (GetLocalViewer() != nullptr);

		if (bApplyWallSlide)
		{
			if (UTCharacterMovement && UTCharacterMovement->WallRunMaterial)
			{
				EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(UTCharacterMovement->WallRunMaterial);
				USoundBase* NewFootStepSound = GetFootstepSoundForSurfaceType(SurfaceType, bLocalViewer);
				if (NewFootStepSound)
				{
					FootstepSoundToPlay = NewFootStepSound;
				}
			}
		}
		else
		{
			static FName NAME_FootstepTrace(TEXT("FootstepTrace"));
			FCollisionQueryParams QueryParams(NAME_FootstepTrace, false, this);
			QueryParams.bReturnPhysicalMaterial = true;
			QueryParams.bTraceAsyncScene = true;
			float PawnRadius, PawnHalfHeight;
			GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
			const FVector LineTraceStart = GetCapsuleComponent()->GetComponentLocation();
			const float TraceDist = 40.0f + PawnHalfHeight;
			const FVector Down = FVector(0.f, 0.f, -TraceDist);

			FHitResult Hit(1.f);
			bool bBlockingHit = GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, GetCapsuleComponent()->GetCollisionObjectType(), QueryParams);
			if (bBlockingHit)
			{
				if (Hit.PhysMaterial.IsValid())
				{
					EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
					USoundBase* NewFootStepSound = GetFootstepSoundForSurfaceType(SurfaceType, bLocalViewer);
					if (NewFootStepSound)
					{
						FootstepSoundToPlay = NewFootStepSound;
					}
				}
			}
		}

		if (bLocalViewer)
		{
			FootStepEffect = GetLocalViewer()->IsBehindView() && (GetVelocity().Size() > 500.f) ? GroundFootstepEffect : NULL;
		}
		else
		{
			FootStepEffect = (GetVelocity().Size() > 500.f) ? GroundFootstepEffect : NULL;
		}
	}

	UUTGameplayStatics::UTPlaySound(GetWorld(), FootstepSoundToPlay, this, SRT_IfSourceNotReplicated, false, FVector::ZeroVector, NULL, NULL, false, SAT_Footstep);

	if (FootStepEffect && GetMesh() && (GetWorld()->GetTimeSeconds() - GetMesh()->LastRenderTime < 0.05f)
		&& (GetLocalViewer() || (GetCachedScalabilityCVars().DetailMode != 0)))
	{
		AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
		if (WS->EffectIsRelevant(this, GetActorLocation(), true, true, MaxParticleDist, 0.f, false))
		{
			FVector EffectLocation = GetActorLocation();
			EffectLocation.Z = EffectLocation.Z + 4.f - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FootStepEffect, EffectLocation, GetActorRotation(), true);
		}
	}
	LastFoot = FootNum;
	LastFootstepTime = GetWorld()->TimeSeconds;
}

float AUTCharacter::GetEyeOffsetScaling() const
{
	float EyeOffsetGlobalScaling = Cast<AUTPlayerController>(GetController()) ? Cast<AUTPlayerController>(GetController())->EyeOffsetGlobalScaling : 1.f;
	return FMath::Clamp(EyeOffsetGlobalScaling, 0.f, 1.f);
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
	return GetEyeOffsetScaling() * (XTransform + ViewRotMatrix.GetScaledAxis(EAxis::Y) * EyeOffset.Y) + FVector(0.f, 0.f, EyeOffset.Z);
}

FVector AUTCharacter::GetPawnViewLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight) + CrouchEyeOffset + GetTransformedEyeOffset();
}

void AUTCharacter::CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult)
{
	if (bFindCameraComponentWhenViewTarget && CharacterCameraComponent && CharacterCameraComponent->bIsActive)
	{
		// don't allow FOV override, we handle that in UTPlayerController/UTPlayerCameraManager
		float SavedFOV = OutResult.FOV;
		const FRotator PawnViewRotation = GetViewRotation();
		if (!PawnViewRotation.Equals(CharacterCameraComponent->GetComponentRotation()))
		{
			CharacterCameraComponent->SetWorldRotation(PawnViewRotation);
		}
			
		CharacterCameraComponent->GetCameraView(DeltaTime, OutResult);
		OutResult.FOV = SavedFOV;
		OutResult.Location = OutResult.Location + CrouchEyeOffset + GetTransformedEyeOffset();
	}
	else
	{
		GetActorEyesViewPoint(OutResult.Location, OutResult.Rotation);
	}
}

void AUTCharacter::PlayJump_Implementation(const FVector& JumpLocation, const FVector& JumpDir)
{
	DesiredJumpBob = WeaponJumpBob;
	TargetEyeOffset.Z = EyeOffsetJumpBob * GetEyeOffsetScaling();
	UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->JumpSound, this, SRT_IfSourceNotReplicated, false, JumpLocation);
}

void AUTCharacter::Falling()
{
	FallingStartTime = GetWorld()->GetTimeSeconds();
}

void AUTCharacter::OnWallDodge_Implementation(const FVector& DodgeLocation, const FVector &DodgeDir)
{
	OnDodge_Implementation(DodgeLocation, DodgeDir);
	if ((DodgeEffect != NULL) && GetCharacterMovement() && !GetCharacterMovement()->IsSwimming())
	{
		FVector EffectLocation = DodgeLocation - DodgeDir * GetCapsuleComponent()->GetUnscaledCapsuleRadius();
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DodgeEffect, DodgeLocation, DodgeDir.Rotation(), true);
	}
}

void AUTCharacter::OnDodge_Implementation(const FVector& DodgeLocation, const FVector &DodgeDir)
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
		UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->SwimPushSound, this, SRT_IfSourceNotReplicated, false, DodgeLocation);
	}
	else
	{
		UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->DodgeSound, this, SRT_IfSourceNotReplicated, false, DodgeLocation);
	}
}

void AUTCharacter::OnSlide_Implementation(const FVector & SlideLocation, const FVector &SlideDir)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->FloorSlideSound, this, SRT_IfSourceNotReplicated, false, SlideLocation);

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
	if ((SlideEffect != NULL) && (GetNetMode() != NM_DedicatedServer))
	{
		UGameplayStatics::SpawnEmitterAttached(SlideEffect, GetRootComponent(), NAME_None, SlideLocation, SlideDir.Rotation(), EAttachLocation::KeepWorldPosition);
	}
}

void AUTCharacter::PlayLandedEffect_Implementation()
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->LandingSound, this, SRT_None);
	UParticleSystem* EffectToPlay = ((GetNetMode() != NM_DedicatedServer) && (FMath::Abs(GetCharacterMovement()->Velocity.Z)) > LandEffectSpeed) ? LandEffect : NULL;
	AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
	if ((EffectToPlay != nullptr) && WS->EffectIsRelevant(this, GetActorLocation(), true, true, 10000.f, 0.f, false))
	{
		FRotator EffectRot = GetCharacterMovement()->CurrentFloor.HitResult.Normal.Rotation();
		EffectRot.Pitch -= 90.f;
		UParticleSystemComponent* LandedPSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), EffectToPlay, GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - 4.f), EffectRot, true);
		float EffectScale = FMath::Clamp(FMath::Square(GetCharacterMovement()->Velocity.Z) / (2.f*LandEffectSpeed*LandEffectSpeed), 0.5f, 1.f);
		LandedPSC->SetRelativeScale3D(FVector(EffectScale, EffectScale, 1.f));
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
				FUTPointDamageEvent DamageEvent(Damage, Hit, -GetCharacterMovement()->Velocity.GetSafeNormal(), CrushingDamageType);
				Hit.Actor->TakeDamage(Damage, DamageEvent, Controller, this);
			}
		}

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
			TargetEyeOffset.Z = EyeOffsetLandBob * FMath::Min(1.f, (-1.f*GetCharacterMovement()->Velocity.Z - (0.8f*EyeOffsetLandBobThreshold)) / FullEyeOffsetLandBobVelZ) * GetEyeOffsetScaling();
		}

		TakeFallingDamage(Hit, GetCharacterMovement()->Velocity.Z);
	}
	OldZ = GetActorLocation().Z;

	Super::Landed(Hit);

	if (!bClientUpdating)
	{
		InventoryEvent(InventoryEventName::Landed);

		LastHitBy = NULL;

		if (UTCharacterMovement && UTCharacterMovement->bIsFloorSliding)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->FloorSlideSound, this, SRT_None);
			if (FeetAreInWater())
			{
				PlayWaterSound(CharacterData.GetDefaultObject()->WaterEntrySound);
			}
		}
		else if (FeetAreInWater())
		{
			if ( PlayWaterSound(CharacterData.GetDefaultObject()->WaterEntrySound) )
			{
				PlayWaterEntryEffect(GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), GetActorLocation());
			}
		}
		else
		{
			PlayLandedEffect();
		}

		AUTBot* B = Cast<AUTBot>(Controller);
		if (B != NULL)
		{
			B->NotifyLanded(Hit);
		}
	}
}

void AUTCharacter::EnteredWater(AUTWaterVolume* WaterVolume)
{
	if ( UTCharacterMovement && PlayWaterSound(WaterVolume->EntrySound ? WaterVolume->EntrySound : CharacterData.GetDefaultObject()->WaterEntrySound) )
	{
		if ((FMath::Abs(UTCharacterMovement->Velocity.Z) > UTCharacterMovement->MaxWaterSpeed) && IsLocallyControlled() && Cast<APlayerController>(GetController()))
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->FastWaterEntrySound, this, SRT_None);
		}
		UTCharacterMovement->Velocity.Z *= WaterVolume->PawnEntryVelZScaling;
		UTCharacterMovement->BrakingDecelerationSwimming = WaterVolume->BrakingDecelerationSwimming;
		PlayWaterEntryEffect(GetActorLocation(), GetActorLocation() + FVector(0.f, 0.f, 100.f));
	}
}

void AUTCharacter::PlayWaterEntryEffect(const FVector& InWaterLoc, const FVector& OutofWaterLoc)
{
	if (GetMesh() && (GetWorld()->GetTimeSeconds() - GetMesh()->LastRenderTime < 0.05f)
		&& (GetCachedScalabilityCVars().DetailMode != 0) )
	{
		AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
		if (WS->EffectIsRelevant(this, GetActorLocation(), true, true, 10000.f, 0.f, false))
		{
			FVector EffectLocation = UTCharacterMovement->FindWaterLine(InWaterLoc, OutofWaterLoc);
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), WaterEntryEffect, EffectLocation, GetActorRotation(), true);
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

void AUTCharacter::CheckRagdollFallingDamage(const FHitResult& Hit)
{
	if (TopRagdollSpeed > MaxSafeFallSpeed)
	{
		FVector SavedVelocity = GetCharacterMovement()->Velocity;
		GetCharacterMovement()->Velocity = FVector(0.0f, 0.0f, -TopRagdollSpeed);
		TakeFallingDamage(Hit, GetCharacterMovement()->Velocity.Z);
		GetCharacterMovement()->Velocity = SavedVelocity;
		// reset so we don't trigger again off collision events in the same frame
		TopRagdollSpeed = 0.0f;
	}
}

void AUTCharacter::OnRagdollCollision(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (IsDead())
	{
		if (NormalImpulse.SizeSquared() > RagdollCollisionBleedThreshold * RagdollCollisionBleedThreshold)
		{
			//UE_LOG(LogUTCharacter, Log, TEXT("RagdollCollision %f %f %f"), NormalImpulse.X, NormalImpulse.Y, NormalImpulse.Z);

			// maybe spawn blood as the ragdoll smacks into things
			if (OtherComp != NULL && OtherActor != this && GetWorld()->TimeSeconds - LastDeathDecalTime > 0.25f && GetWorld()->TimeSeconds - GetLastRenderTime() < 1.0f)
			{
				SpawnBloodDecal(GetActorLocation(), -Hit.Normal);
				LastDeathDecalTime = GetWorld()->TimeSeconds;
			}
		}
	}
	// cause falling damage on Z axis collisions
	else if (!bInRagdollRecovery)
	{
		CheckRagdollFallingDamage(Hit);
	}
}

void AUTCharacter::SetCharacterOverlay(UMaterialInterface* NewOverlay, bool bEnabled)
{
	SetCharacterOverlayEffect(FOverlayEffect(NewOverlay), bEnabled);
}
void AUTCharacter::SetCharacterOverlayEffect(const struct FOverlayEffect& NewOverlay, bool bEnabled)
{
	if (Role == ROLE_Authority && NewOverlay.IsValid())
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			int32 Index = GS->FindOverlayEffect(NewOverlay);
			if (Index == INDEX_NONE)
			{
				UE_LOG(UT, Warning, TEXT("Overlay effect %s was not registered"), *NewOverlay.ToString());
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
	SetWeaponOverlayEffect(FOverlayEffect(NewOverlay), bEnabled);
}
void AUTCharacter::SetWeaponOverlayEffect(const FOverlayEffect& NewOverlay, bool bEnabled)
{
	if (Role == ROLE_Authority && NewOverlay.IsValid())
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS != NULL)
		{
			int32 Index = GS->FindOverlayEffect(NewOverlay);
			if (Index == INDEX_NONE)
			{
				UE_LOG(UT, Warning, TEXT("Overlay effect %s was not registered"), *NewOverlay.ToString());
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

void AUTCharacter::UpdateCharOverlayFlags()
{
	UpdateCharOverlays();
	UpdateArmorOverlay();
}

void AUTCharacter::UpdateCharOverlays()
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (CharOverlayFlags == 0)
	{
		if (OverlayMesh != NULL && OverlayMesh->IsRegistered())
		{
			OverlayMesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			OverlayMesh->UnregisterComponent();
			const TArray<USceneComponent*> ChildrenCopy = OverlayMesh->GetAttachChildren();
			for (USceneComponent* Child : ChildrenCopy)
			{
				UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Child);
				if (PSC != NULL && PSC->IsActive())
				{
					PSC->bAutoDestroy = true;
					PSC->DeactivateSystem();
					PSC->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
				}
				else
				{
					Child->DestroyComponent(false);
				}
			}

			if (LeaderHat)
			{
				LeaderHat->SetActorHiddenInGame(false);
			}
			else if (Hat)
			{
				Hat->SetActorHiddenInGame(false);
			}
		}
	}
	else if (GS != NULL)
	{
		if (OverlayMesh == NULL)
		{
			OverlayMesh = DuplicateObject<USkeletalMeshComponent>(GetMesh(), this);
			OverlayMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform); // AttachParent gets copied but we don't want it to be
			OverlayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // make sure because could be in ragdoll
			OverlayMesh->SetSimulatePhysics(false);
			OverlayMesh->SetCastShadow(false);
			OverlayMesh->SetMasterPoseComponent(GetMesh());
			OverlayMesh->BoundsScale = 15000.f;
			OverlayMesh->InvalidateCachedBounds();
			OverlayMesh->UpdateBounds();
			OverlayMesh->bVisible = true;
			OverlayMesh->bHiddenInGame = false;
		}
		if (!OverlayMesh->IsRegistered())
		{
			OverlayMesh->RegisterComponent();
			OverlayMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			OverlayMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
			OverlayMesh->LastRenderTime = GetMesh()->LastRenderTime;
		}

		FOverlayEffect FirstOverlay = GS->GetFirstOverlay(CharOverlayFlags, false);
		// note: MID doesn't have any safe way to change Parent at runtime, so we need to make a new one every time...
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FirstOverlay.Material, OverlayMesh);
		// apply team color, if applicable
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		if (PS != NULL && PS->Team != NULL)
		{
			static FName NAME_TeamColor(TEXT("TeamColor"));
			MID->SetVectorParameterValue(NAME_TeamColor, FVector(1.f, 1.f, 0.f));
		}
		for (int32 i = 0; i < OverlayMesh->GetNumMaterials(); i++)
		{
			OverlayMesh->SetMaterial(i, MID);
		}
		if (FirstOverlay.Particles != NULL)
		{
			UParticleSystemComponent* PSC = NULL;
			for (USceneComponent* Child : OverlayMesh->GetAttachChildren())
			{
				PSC = Cast<UParticleSystemComponent>(Child);
				if (PSC != NULL)
				{
					break;
				}
			}
			if (PSC == NULL)
			{
				PSC = NewObject<UParticleSystemComponent>(OverlayMesh);
				PSC->RegisterComponent();
			}
			PSC->AttachToComponent(OverlayMesh, FAttachmentTransformRules::KeepRelativeTransform, FirstOverlay.ParticleAttachPoint);
			PSC->SetTemplate(FirstOverlay.Particles);
		}
		else
		{
			for (USceneComponent* Child : OverlayMesh->GetAttachChildren())
			{
				UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Child);
				if (PSC != NULL)
				{
					if (PSC->IsActive())
					{
						PSC->bAutoDestroy = true;
						PSC->DeactivateSystem();
						PSC->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
					}
					else
					{
						PSC->DestroyComponent();
					}
					break;
				}
			}
		}

		if (LeaderHat && LeaderHat->bHideWithOverlay)
		{
			LeaderHat->SetActorHiddenInGame(true);
		}
		if (Hat && Hat->bHideWithOverlay)
		{
			Hat->SetActorHiddenInGame(true);
		}
	}
}

void AUTCharacter::SetOutlineServer(bool bNowOutlined, bool bWhenUnoccluded, uint8 TeamMask)
{
	if (Role == ROLE_Authority)
	{
		// outline not allowed on corpses
		if (IsDead())
		{
			bServerOutline = false;
			bLocalOutline = false;
		}
		else
		{
			if (TeamMask != 0)
			{
				if (bNowOutlined)
				{
					ServerOutlineTeamMask |= TeamMask;
				}
				else
				{
					ServerOutlineTeamMask &= ~TeamMask;
				}
				bServerOutline = (ServerOutlineTeamMask != 0);
			}
			else
			{
				bServerOutline = bNowOutlined;
				if (!bNowOutlined)
				{
					ServerOutlineTeamMask = 0;
				}
			}
		}
		bOutlineWhenUnoccluded = bWhenUnoccluded;

		if (GetNetMode() != NM_DedicatedServer)
		{
			UpdateOutline();
		}
	}
}
void AUTCharacter::SetOutlineLocal(bool bNowOutlined, bool bWhenUnoccluded)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		// outline not allowed on corpses
		if (IsDead())
		{
			bServerOutline = false;
			bLocalOutline = false;
		}
		else
		{
			bLocalOutline = bNowOutlined;
		}
		// TODO: this should be server only, need to refactor flag carrier outlining
		bOutlineWhenUnoccluded = bWhenUnoccluded;

		UpdateOutline();
	}
}

void AUTCharacter::UpdateOutline()
{
	const bool bOutlined = IsOutlined();
	// 0 is a null value for the stencil so use team + 1
	// last bit in stencil is a bitflag so empty team uses 127
	uint8 NewStencilValue = (GetTeamNum() == 255) ? 127 : (GetTeamNum() + 1);
	if (bOutlineWhenUnoccluded)
	{
		NewStencilValue |= 128;
	}
	if (bOutlined)
	{
		GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;
		if (CustomDepthMesh == NULL)
		{
			CustomDepthMesh = Cast<USkeletalMeshComponent>(CreateCustomDepthOutlineMesh(GetMesh(), this));
		}
		if (CustomDepthMesh->CustomDepthStencilValue != NewStencilValue)
		{
			CustomDepthMesh->CustomDepthStencilValue = NewStencilValue;
			CustomDepthMesh->MarkRenderStateDirty();
		}
		if (!CustomDepthMesh->IsRegistered())
		{
			CustomDepthMesh->RegisterComponent();
			CustomDepthMesh->LastRenderTime = GetMesh()->LastRenderTime;
		}
	}
	else
	{
		if (TauntCount == 0) // if taunting need this on until taunt is done for timing purposes
		{
			GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
		}
		if (CustomDepthMesh != NULL && CustomDepthMesh->IsRegistered())
		{
			CustomDepthMesh->UnregisterComponent();
		}
	}
	if (Weapon != NULL)
	{
		Weapon->UpdateOutline();
	}
	if (WeaponAttachment != NULL)
	{
		WeaponAttachment->UpdateOutline(bOutlined, NewStencilValue);
	}
	if (GetCarriedObject() != NULL)
	{
		GetCarriedObject()->UpdateOutline();
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

void AUTCharacter::SetSkin(UMaterialInterface* NewSkin, UMaterialInterface* NewSkin1P)
{
	ReplicatedBodyMaterial = NewSkin;
	ReplicatedBodyMaterial1P = (NewSkin != NULL) ? NewSkin1P : NULL;
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
		for (int32 i = 0; i < FirstPersonMesh->GetNumMaterials(); i++)
		{
			FirstPersonMesh->SetMaterial(i, (ReplicatedBodyMaterial1P != NULL) ? ReplicatedBodyMaterial1P : ReplicatedBodyMaterial);
		}
	}
	else
	{
		for (int32 i = 0; i < GetMesh()->GetNumMaterials(); i++)
		{
			GetMesh()->SetMaterial(i, BodyMIs.IsValidIndex(i) ? BodyMIs[i] : GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->GetMaterial(i));
		}
		for (int32 i = 0; i < FirstPersonMesh->GetNumMaterials(); i++)
		{
			FirstPersonMesh->SetMaterial(i, GetClass()->GetDefaultObject<AUTCharacter>()->FirstPersonMesh->GetMaterial(i));
		}
	}
	if (Weapon != NULL)
	{
		Weapon->SetSkin((ReplicatedBodyMaterial1P != NULL) ? ReplicatedBodyMaterial1P : ReplicatedBodyMaterial);
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

AUTPlayerController* AUTCharacter::GetLocalViewer()
{
	if (CurrentViewerPC && ((Controller == CurrentViewerPC) || (CurrentViewerPC->GetViewTarget() == this)))
	{
		return CurrentViewerPC;
	}
	CurrentViewerPC = NULL;
	for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
	{
		if (It->PlayerController != NULL && It->PlayerController->GetViewTarget() == this)
		{
			CurrentViewerPC = Cast<AUTPlayerController>(It->PlayerController);
			break;
		}
	}
	return CurrentViewerPC;
}

void AUTCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (IsRagdoll())
	{
		TopRagdollSpeed = FMath::Max<float>(TopRagdollSpeed, GetMesh()->GetComponentVelocity().Size());
	}

	if (Role == ROLE_Authority)
	{
		bIsInCombat = (GetWorld()->GetTimeSeconds() - FMath::Max(LastTargetingTime, LastTargetedTime) < 2.5f);

		// If have currently pinged flag carrier target, see if need visual check to extend pinged time.
		AUTCarriedObject* TargetedFlag = LastTarget ? LastTarget->GetCarriedObject() : nullptr;
		if (bHaveTargetVisual && GetController() && TargetedFlag && TargetedFlag->bShouldPingFlag && TargetedFlag->bCurrentlyPinged && (GetWorld()->GetTimeSeconds() - LastTargetingTime < TargetedFlag->PingedDuration) && (GetWorld()->GetTimeSeconds() - LastTarget->LastTargetSeenTime > TargetedFlag->TargetPingedDuration - 0.2f))
		{
			bHaveTargetVisual = false;
			FVector Viewpoint = GetActorLocation();
			Viewpoint.Z += BaseEyeHeight;
			FVector TargetDir = (LastTarget->GetActorLocation() - Viewpoint).GetSafeNormal();
			FVector TargetHeadLoc = LastTarget->GetActorLocation() + FVector(0.0f, 0.0f, LastTarget->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
			FVector ViewDir = GetController()->GetControlRotation().Vector();
			if (((ViewDir | TargetDir) > 0.9f) || ((ViewDir | (TargetHeadLoc - Viewpoint).GetSafeNormal()) > 0.9f))
			{
				FCollisionQueryParams TraceParams(FName(TEXT("ChooseBestAimTarget")), false);
				bool bHit = GetWorld()->LineTraceTestByChannel(Viewpoint, TargetHeadLoc, COLLISION_TRACE_WEAPONNOCHARACTER, TraceParams);
				if (bHit)
				{
					bHit = GetWorld()->LineTraceTestByChannel(Viewpoint, LastTarget->GetActorLocation(), COLLISION_TRACE_WEAPONNOCHARACTER, TraceParams);
				}
				if (!bHit)
				{
					bHaveTargetVisual = true;
					LastTarget->LastTargetSeenTime = GetWorld()->GetTimeSeconds();
				}
			}
		}

		if ((GetNetMode() == NM_Standalone) || (GetNetMode() == NM_ListenServer))
		{
			SetRemoteViewPitch(GetControlRotation().Pitch);
		}
	}
	if (HeadScale < 0.1f)
	{
		GetMesh()->ClothBlendWeight = 0.0f;
	}
	else if (GetMovementBase() && Cast<AUTLift>(GetMovementBase()->GetOwner()) && (GetMovementBase()->GetOwner()->GetVelocity().Z >= 0.f))
	{
		GetMesh()->ClothBlendWeight = 0.5f;
	}
	else
	{
		GetMesh()->ClothBlendWeight = 1.0f;
	}

	if (GetMesh()->MeshComponentUpdateFlag >= EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered && !GetMesh()->bRecentlyRendered && (!IsLocallyControlled() || !Cast<APlayerController>(GetController()))
		&& (bApplyWallSlide || GetCharacterMovement()->MovementMode == MOVE_Walking) && !bFeigningDeath && !IsDead())
	{
		// TODO: currently using an arbitrary made up interval and scale factor
		float Speed = GetCharacterMovement()->Velocity.Size();
		if (Speed > 0.0f && GetWorld()->TimeSeconds - LastFootstepTime > 0.35f * GetCharacterMovement()->MaxWalkSpeed / Speed)
		{
			PlayFootstep((LastFoot + 1) & 1, true);
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
	if (OverlayMesh != NULL && OverlayMesh->IsRegistered())
	{
		// FIXME: workaround for engine bug with belt material not rendering
		OverlayMesh->MarkRenderStateDirty();

		// FIXME: temp hack for showdown prototype
		bool bSendHealthToOverlay = false;
		for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
		{
			if (It->PlayerController != NULL && It->PlayerController->PlayerState != NULL && It->PlayerController->PlayerState->bOnlySpectator)
			{
				bSendHealthToOverlay = true;
				break;
			}
		}
		if (bSendHealthToOverlay)
		{
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(OverlayMesh->GetMaterial(0));
			if (MID != NULL)
			{
				static FName NAME_Damage(TEXT("Damage"));
				MID->SetScalarParameterValue(NAME_Damage, 0.01f * (100.f - FMath::Clamp<float>(Health, 0.f, 100.f)));
			}
		}
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
			GetMesh()->PutAllRigidBodiesToSleep(); // make sure since we can't disable the physics without it breaking
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
			GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);
			GetMesh()->SetRelativeLocation(GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeLocation);
			GetMesh()->SetRelativeRotation(GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeRotation);
			GetMesh()->SetRelativeScale3D(GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeScale3D);

			if (!GS || !GS->IsMatchIntermission() || !GS->HasMatchEnded())
			{
				DisallowWeaponFiring(GetClass()->GetDefaultObject<AUTCharacter>()->bDisallowWeaponFiring);
			}
		}
	}

	// update eyeoffset 
	if (GetCharacterMovement()->MovementMode == MOVE_Walking && !MovementBaseUtility::UseRelativeLocation(BasedMovement.MovementBase))
	{
		// smooth up/down stairs
		if (GetCharacterMovement()->bJustTeleported && (FMath::Abs(OldZ - GetActorLocation().Z) > GetCharacterMovement()->MaxStepHeight))
		{
//			UE_LOG(UT, Warning, TEXT("TELEP"));
			EyeOffset.Z = 0.f;
		}
		else
		{
			EyeOffset.Z += (OldZ - GetActorLocation().Z);
		}

		// avoid clipping
		if (CrouchEyeOffset.Z + EyeOffset.Z > GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - BaseEyeHeight - 12.f)
		{
			if (!GetLocalViewer())
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
				if (GetWorld()->SweepSingleByChannel(Hit, GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight), GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight) + CrouchEyeOffset + GetTransformedEyeOffset(), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(12.f), Params))
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
	OldZ = GetActorLocation().Z;

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
	float OldEyeOffsetZ = EyeOffset.Z;
	EyeOffset.X = (1.f - InterpTimeX)*EyeOffset.X + InterpTimeX*TargetEyeOffset.X;
	EyeOffset.Y = (1.f - InterpTimeY)*EyeOffset.Y + InterpTimeY*TargetEyeOffset.Y;
	EyeOffset.Z = (1.f - InterpTimeZ)*EyeOffset.Z + InterpTimeZ*TargetEyeOffset.Z;

	EyeOffset.X = (TargetEyeOffset.X > EyeOffset.X) 
					? EyeOffset.X + FMath::Min(TargetEyeOffset.X - EyeOffset.X, DeltaTime*EyeOffsetDirectRate.X)
					: EyeOffset.X + FMath::Max(TargetEyeOffset.X - EyeOffset.X, -1.f * DeltaTime*EyeOffsetDirectRate.X);
	EyeOffset.Y = (TargetEyeOffset.Y > EyeOffset.Y)
					? EyeOffset.Y + FMath::Min(TargetEyeOffset.Y - EyeOffset.Y, DeltaTime*EyeOffsetDirectRate.Y)
					: EyeOffset.Y + FMath::Max(TargetEyeOffset.Y - EyeOffset.Y, -1.f * DeltaTime*EyeOffsetDirectRate.Y);

	float ZChange = FMath::Abs(EyeOffset.Z - OldEyeOffsetZ);
	if (ZChange < DeltaTime*EyeOffsetDirectRate.Z)
	{
		EyeOffset.Z = (TargetEyeOffset.Z > EyeOffset.Z)
			? EyeOffset.Z + FMath::Min(TargetEyeOffset.Z - EyeOffset.Z, DeltaTime*EyeOffsetDirectRate.Z - ZChange)
			: EyeOffset.Z + FMath::Max(TargetEyeOffset.Z - EyeOffset.Z, -1.f * DeltaTime*EyeOffsetDirectRate.Z - ZChange);
	}
	float CrouchInterpTime = FMath::Min(1.f, CrouchEyeOffsetInterpRate*DeltaTime);
	CrouchEyeOffset = (1.f - CrouchInterpTime)*CrouchEyeOffset; 
	if (EyeOffset.Z > 0.f)
	{
		// faster decay if positive to avoid camera clipping going down stairs
		EyeOffset.Z = (1.f - InterpTimeZ)*EyeOffset.Z + InterpTimeZ*TargetEyeOffset.Z;
	}
	EyeOffset.DiagnosticCheckNaN();
//	UE_LOG(UT, Warning, TEXT("EyeOffset %f"), EyeOffset.Z);
	TargetEyeOffset.X *= FMath::Max(0.f, 1.f - FMath::Min(1.f, EyeOffsetDecayRate.X*DeltaTime));
	TargetEyeOffset.Y *= FMath::Max(0.f, 1.f - FMath::Min(1.f, EyeOffsetDecayRate.Y*DeltaTime));
	TargetEyeOffset.Z *= FMath::Max(0.f, 1.f - FMath::Min(1.f, EyeOffsetDecayRate.Z*DeltaTime));
	TargetEyeOffset.DiagnosticCheckNaN();

	if (GetWeapon())
	{
		GetWeapon()->UpdateViewBob(DeltaTime);
	}
	else
	{
		GetWeaponBobOffset(DeltaTime, NULL);
	}
	AUTPlayerController* MyPC = GetLocalViewer();
	if (GS && GS->IsMatchIntermission())
	{
		SetStatusAmbientSound(NULL);
		SetLocalAmbientSound(NULL);
	}
	else if (MyPC && GetCharacterMovement()) 
	{
		if (MyPC && MyPC->UTPlayerState && MyPC->UTPlayerState->CarriedObject && MyPC->UTPlayerState->CarriedObject->HeldFlagAmbientSound)
		{
			SetStatusAmbientSound(MyPC->UTPlayerState->CarriedObject->HeldFlagAmbientSound, 1.f, 1.f, false);
		}
		else if ((Health <= LowHealthAmbientThreshold) && (Health > 0))
		{
			float UrgencyFactor = (LowHealthAmbientThreshold - Health) / LowHealthAmbientThreshold;
			SetStatusAmbientSound(LowHealthAmbientSound, 0.5f + FMath::Clamp<float>(UrgencyFactor, 0.f, 1.f), 1.f, false);
		}
		else
		{
			StatusAmbientSound = NULL;
			StatusAmbientSoundUpdated();
		}
		// @TODO FIXMESTEVE this should all be event driven
		if (GetCharacterMovement()->IsFalling() && (GetCharacterMovement()->Velocity.Z < FallingAmbientStartSpeed))
		{
			SetLocalAmbientSound(FallingAmbientSound, FMath::Clamp((FallingAmbientStartSpeed - GetCharacterMovement()->Velocity.Z) / (1.5f*MaxSafeFallSpeed + FallingAmbientStartSpeed), 0.f, 1.5f), false);
		}
		else
		{
			SetLocalAmbientSound(FallingAmbientSound, 0.f, true);
			if (UTCharacterMovement && UTCharacterMovement->bSlidingAlongWall)
			{
				SetLocalAmbientSound(WallSlideAmbientSound, 1.f, false);
			}
			else if (GetCharacterMovement()->IsMovingOnGround() && (GetCharacterMovement()->Velocity.Size2D() > SprintAmbientStartSpeed))
			{
				float NewLocalAmbientVolume = FMath::Min(1.f, (GetCharacterMovement()->Velocity.Size2D() - SprintAmbientStartSpeed) / (UTCharacterMovement->SprintSpeed - SprintAmbientStartSpeed));
				LocalAmbientVolume = LocalAmbientVolume*(1.f - DeltaTime) + NewLocalAmbientVolume*DeltaTime;
				SetLocalAmbientSound(CharacterData.GetDefaultObject()->SprintAmbientSound, LocalAmbientVolume, false);
			}
			else if ((LocalAmbientSound == CharacterData.GetDefaultObject()->SprintAmbientSound) && (LocalAmbientVolume > 0.05f))
			{
				LocalAmbientVolume = LocalAmbientVolume*(1.f - DeltaTime);
				SetLocalAmbientSound(CharacterData.GetDefaultObject()->SprintAmbientSound, LocalAmbientVolume, false);
			}
			else
			{
				SetLocalAmbientSound(WallSlideAmbientSound, 0.f, true);
				SetLocalAmbientSound(CharacterData.GetDefaultObject()->SprintAmbientSound, 0.f, true);
			}
		}
	}
	else
	{
		SetStatusAmbientSound(LowHealthAmbientSound, 0.f, 1.f, true);
	}

	if (IsInWater())
	{
		if (IsRagdoll() && !bInRagdollRecovery && GetMesh() && (!GS || GS->IsMatchInProgress()))
		{
			// apply force to fake buoyancy and fluid friction
			float FloatMag = (IsDead() || !PositionIsInWater(GetMesh()->GetBoneLocation(TEXT("neck_01")) +FVector(0.f, 0.f, 10.f))) ? 110.f : 190.f;
			FVector FluidForce = -500.f * GetVelocity() - FVector(0.f, 0.f, FloatMag*GetWorld()->GetGravityZ());

			// also apply any current force
			FVector SpineLoc = GetMesh()->GetBoneLocation(TEXT("spine_02"));
			AUTWaterVolume* WaterVolume = Cast<AUTWaterVolume>(PositionIsInWater(GetActorLocation()));
			if (WaterVolume)
			{
				FluidForce += 3.f * FloatMag * WaterVolume->GetCurrentFor(this);
			}

			GetMesh()->AddForce(0.3f*FluidForce, FName(TEXT("spine_02")));
			GetMesh()->AddForce(0.1f*FluidForce);
			GetMesh()->AddForce(0.1f*FluidForce, FName(TEXT("spine_03")));
			GetMesh()->AddForce(0.07f*FluidForce, FName(TEXT("neck_01")));

			GetMesh()->AddForce(0.025f*FluidForce + 2500.f * (GetMesh()->GetBoneLocation(TEXT("lowerarm_l")) - SpineLoc).GetSafeNormal2D(), FName((TEXT("lowerarm_l"))));
			GetMesh()->AddForce(0.025f*FluidForce + 2500.f * (GetMesh()->GetBoneLocation(TEXT("lowerarm_r")) - SpineLoc).GetSafeNormal2D(), FName((TEXT("lowerarm_r"))));
			GetMesh()->AddForce(0.04f*FluidForce + 5000.f * (GetMesh()->GetBoneLocation(TEXT("foot_l")) - SpineLoc).GetSafeNormal2D(), FName((TEXT("foot_l"))));
			GetMesh()->AddForce(0.04f*FluidForce + 5000.f * (GetMesh()->GetBoneLocation(TEXT("foot_r")) - SpineLoc).GetSafeNormal2D(), FName((TEXT("foot_r"))));
		}
		if ((Role == ROLE_Authority) && (Health > 0))
		{
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
					UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->GaspSound, this, SRT_None);
				}
				LastBreathTime = GetWorld()->GetTimeSeconds();
			}
		}
	}
	else
	{
		if (IsRagdoll() && !bInRagdollRecovery && RagdollGravityScale != 0.0f && RagdollGravityScale != 1.0f)
		{
			// apply force to add or remove from the standard gravity force (that we can't modify on an individual object)
			for (FBodyInstance* Body : GetMesh()->Bodies)
			{
				Body->AddForce(FVector(0.0f, 0.0f, GetWorld()->GetGravityZ() * -(1.0f - RagdollGravityScale)), true, true);
			}
		}

		LastBreathTime = GetWorld()->GetTimeSeconds();
	}
	/*
	if (CharacterMovement && ((CharacterMovement->GetCurrentAcceleration() | CharacterMovement->Velocity) < 0.f))
	{
	UE_LOG(UT, Warning, TEXT("Position %f %f time %f"),GetActorLocation().X, GetActorLocation().Y, GetWorld()->GetTimeSeconds());
	}*/
}

float AUTCharacter::GetLastRenderTime() const
{
	// ignore special effects (e.g. overlay) using CustomDepth as they will render through walls and we don't want to count them if we can avoid it
	float LastRenderTime = -1000.f;
	for (const UActorComponent* ActorComponent : GetComponents())
	{
		const UPrimitiveComponent* PrimComp = Cast<const UPrimitiveComponent>(ActorComponent);
		if (PrimComp != NULL && PrimComp->IsRegistered() && (!PrimComp->bRenderCustomDepth || PrimComp == GetMesh()))
		{
			LastRenderTime = FMath::Max(LastRenderTime, PrimComp->LastRenderTime);
		}
	}
	return LastRenderTime;
}

bool AUTCharacter::IsInWater() const
{
	if (IsRagdoll())
	{
		return (PositionIsInWater(GetActorLocation()) != NULL);
	}
	return (GetCharacterMovement() && GetCharacterMovement()->IsInWater());
}

bool AUTCharacter::HeadIsUnderWater() const
{
	FVector HeadLocation = GetActorLocation() + FVector(0.f, 0.f, BaseEyeHeight);
	return (PositionIsInWater(HeadLocation) != NULL);
}

bool AUTCharacter::FeetAreInWater() const
{
	FVector FootLocation = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	return (PositionIsInWater(FootLocation) != NULL);
}

APhysicsVolume* AUTCharacter::PositionIsInWater(const FVector& Position) const
{
	// check for all volumes that overlapposition
	APhysicsVolume* NewVolume = NULL;
	TArray<FOverlapResult> Hits;
	static FName NAME_PhysicsVolumeTrace = FName(TEXT("PhysicsVolumeTrace"));
	FComponentQueryParams Params(NAME_PhysicsVolumeTrace, GetOwner());
	GetWorld()->OverlapMultiByChannel(Hits, Position, FQuat::Identity, GetCapsuleComponent()->GetCollisionObjectType(), FCollisionShape::MakeSphere(0.f), Params);

	for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
	{
		const FOverlapResult& Link = Hits[HitIdx];
		APhysicsVolume* const V = Cast<APhysicsVolume>(Link.GetActor());
		if (V && (!NewVolume || (V->Priority > NewVolume->Priority)))
		{
			NewVolume = V;
		}
	}
	return (NewVolume && NewVolume->bWaterVolume) ? NewVolume : NULL;
}

void AUTCharacter::TakeDrowningDamage()
{
	FUTPointDamageEvent DamageEvent(DrowningDamagePerSecond, FHitResult(this, GetCapsuleComponent(), GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)), FVector(0.0f, 0.0f, -1.0f), UUTDmgType_Drown::StaticClass());
	TakeDamage(DrowningDamagePerSecond, DamageEvent, Controller, this);
	UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->DrowningSound, this, SRT_None);
}

uint8 AUTCharacter::GetTeamNum() const
{
	static FName NAME_ScriptGetTeamNum(TEXT("ScriptGetTeamNum"));
	UFunction* GetTeamNumFunc = GetClass()->FindFunctionByName(NAME_ScriptGetTeamNum);
	if (GetTeamNumFunc != NULL && GetTeamNumFunc->Script.Num() > 0)
	{
		return IUTTeamInterface::Execute_ScriptGetTeamNum(const_cast<AUTCharacter*>(this));
	}

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

	if (Role == ROLE_Authority)
	{
		SetCosmeticsFromPlayerState();
		AUTGameVolume* GV = UTCharacterMovement ? Cast<AUTGameVolume>(UTCharacterMovement->GetPhysicsVolume()) : nullptr;
		if (GV && GV->bIsTeamSafeVolume)
		{
			AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
			if (GS && GS->OnSameTeam(this, GV))
			{
				bDamageHurtsHealth = false;
			}
		}
	}
	OldPlayerState = Cast<AUTPlayerState>(PlayerState);
}

void AUTCharacter::UnPossessed()
{
	StopFiring();
	SetAmbientSound(NULL);
	SetStatusAmbientSound(NULL);
	SetLocalAmbientSound(NULL);
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
		OldPlayerState = Cast<AUTPlayerState>(PlayerState);
	}

	SetCosmeticsFromPlayerState();
}

void AUTCharacter::SetCosmeticsFromPlayerState()
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS)
	{
		SetHatVariant(PS->HatVariant);
		SetHatClass(PS->OverrideHatClass != nullptr ? PS->OverrideHatClass : PS->HatClass);
		SetEyewearVariant(PS->EyewearVariant);
		SetEyewearClass(PS->EyewearClass);
		bShouldWearLeaderHat = PS->bHasHighScore;
		LeaderHatStatusChanged_Implementation();
	}
}

AUTCharacterContent* AUTCharacter::GetCharacterData() const
{
	return CharacterData.GetDefaultObject();
}

void AUTCharacter::ApplyCharacterData(TSubclassOf<AUTCharacterContent> CharType)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (CharType != NULL)
	{
		CharacterData = CharType;
	}
	const AUTCharacterContent* Data = CharacterData.GetDefaultObject();
	if (Data->Mesh != NULL)
	{
		FComponentReregisterContext ReregisterContext(GetMesh());
		GetMesh()->OverrideMaterials = Data->Mesh->OverrideMaterials;
		FFAColor = (Data->DMSkinType == EDMSkin_Base) ? 255 : 0;
		if ((PS != NULL && PS->Team != NULL) || (FFAColor != 255))
		{
			GetMesh()->OverrideMaterials.SetNumZeroed(FMath::Min<int32>(Data->Mesh->GetNumMaterials(), Data->TeamMaterials.Num()));
			for (int32 i = GetMesh()->OverrideMaterials.Num() - 1; i >= 0; i--)
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
				UMaterialInstanceDynamic* MI = GetMesh()->CreateAndSetMaterialInstanceDynamic(i);
				MI->SetScalarParameterValue(TEXT("TeamSelect"), FFAColor);
				BodyMIs.Add(MI);
			}
		}
		GetMesh()->PhysicsAssetOverride = Data->Mesh->PhysicsAssetOverride;
		GetMesh()->RelativeScale3D = GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->RelativeScale3D * Data->Mesh->RelativeScale3D;
		if (GetMesh() != GetRootComponent())
		{
			// FIXMESTEVE re-enable after fixing content, also need to override startcrouch and endcrouch to use this value
			//GetMesh()->RelativeLocation = Data->Mesh->RelativeLocation;
			GetMesh()->RelativeRotation = Data->Mesh->RelativeRotation;
		}
		// reapply any temporary override effects
		if (OverlayMesh != NULL)
		{
			OverlayMesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			OverlayMesh->UnregisterComponent();
			OverlayMesh = NULL;
			UpdateCharOverlays();
		}
		if (CustomDepthMesh != NULL)
		{
			CustomDepthMesh->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
			CustomDepthMesh->UnregisterComponent();
			CustomDepthMesh = NULL;
			UpdateOutline();
		}
		UpdateSkin();
	}
}

void AUTCharacter::NotifyTeamChanged()
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL)
	{
		ApplyCharacterData(PS->GetSelectedCharacter());
		for (UMaterialInstanceDynamic* MI : BodyMIs)
		{
			if (MI != NULL)
			{
				static FName NAME_TeamColor(TEXT("TeamColor"));
				if ((PS->Team != NULL)  || (FFAColor != 255))
				{
					float SkinSelect = PS->Team ? PS->Team->TeamIndex : FFAColor;
					MI->SetScalarParameterValue(TEXT("TeamSelect"), SkinSelect);
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

		//Update weapon team colors
		if (Weapon != nullptr)
		{
			Weapon->NotifyTeamChanged();
		}
		if (WeaponAttachment != nullptr)
		{
			WeaponAttachment->NotifyTeamChanged();
		}

		// Refresh leader hat
		if (LeaderHat)
		{
			LeaderHat->Destroy();
			LeaderHat = nullptr;
		}
		LeaderHatStatusChanged();
	}
}

void AUTCharacter::PlayerChangedTeam()
{
}

void AUTCharacter::PlayerSuicide()
{
	if (Role == ROLE_Authority)
	{
		FHitResult FakeHit(this, NULL, GetActorLocation(), GetActorRotation().Vector());
		FUTPointDamageEvent FakeDamageEvent(0, FakeHit, FVector(0, 0, 0), UUTDmgType_Suicide::StaticClass());
		UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->PainSound, this, SRT_All, false, FVector::ZeroVector, Cast<AUTPlayerController>(Controller), NULL, false, SAT_PainSound);
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

int32 AUTCharacter::GetArmorAmount() const
{
	return ArmorAmount;
}

void AUTCharacter::GiveArmor(AUTArmor* ArmorClass)
{
	if (ArmorClass == nullptr)
	{
		ArmorClass = AUTGameMode::StaticClass()->GetDefaultObject<AUTGameMode>()->StartingArmorClass.GetDefaultObject();
		if (ArmorClass == nullptr)
		{
			return;
		}
	}

	ArmorType = ArmorClass;
	ArmorAmount = FMath::Max(FMath::Max(ArmorAmount, ArmorClass->ArmorAmount), FMath::Min(100, ArmorAmount + ArmorClass->ArmorAmount));
	ArmorRemovalAssists.Empty();
	UpdateArmorOverlay();
}

void AUTCharacter::RemoveArmor(int32 Amount)
{
	ArmorAmount = FMath::Max(0, ArmorAmount - Amount);
	UpdateArmorOverlay();
}

void AUTCharacter::UpdateArmorOverlay()
{
	if (ArmorType && (ArmorAmount > 50))
	{
		SetCharacterOverlayEffect(ArmorType->OverlayEffect.IsValid() ? ArmorType->OverlayEffect : FOverlayEffect(ArmorType->OverlayMaterial), true);
		UMaterialInstanceDynamic* MID = OverlayMesh ? Cast<UMaterialInstanceDynamic>(OverlayMesh->GetMaterial(0)) : nullptr;
		if (MID)
		{
			static const FName NAME_Fresnel = FName(TEXT("Fresnel"));
			float FresnelValue = (ArmorAmount > 100) ? 15.f : 1.f;
			MID->SetScalarParameterValue(NAME_Fresnel, FresnelValue);
			static const FName NAME_PushDistance = FName(TEXT("PushDistance"));
			float PushValue = (ArmorAmount > 100) ? 4.f : 0.2f;
			MID->SetScalarParameterValue(NAME_PushDistance, PushValue);
			static const FName NAME_Opacity = FName(TEXT("Opacity"));
			float OpacityValue = -0.25f;
			MID->SetScalarParameterValue(NAME_Opacity, OpacityValue);

			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
			if (PS != NULL)
			{
				if (PS->Team != NULL)
				{
					static FName NAME_TeamColor(TEXT("TeamColor"));
					MID->SetVectorParameterValue(NAME_TeamColor, PS->Team->TeamColor);
				}
				FLinearColor BaseColor = (PS->Team && (PS->Team->TeamIndex == 1)) ? FLinearColor(1.f, 1.f, 0.f, 1.f) : FLinearColor(0.5f, 0.5f, 0.1f, 1.f);
				static FName NAME_Color(TEXT("Color"));
				MID->SetVectorParameterValue(NAME_Color, BaseColor);
			}
		}
	}
	else if (ArmorType)
	{
		SetCharacterOverlayEffect(ArmorType->OverlayEffect.IsValid() ? ArmorType->OverlayEffect : FOverlayEffect(ArmorType->OverlayMaterial), false);
	}
}

void AUTCharacter::OV(FName InName, float value)
{
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(OverlayMesh->GetMaterial(0));
	MID->SetScalarParameterValue(InName, value);
}

void AUTCharacter::OVV(FName InName, FVector value)
{
	UE_LOG(UT, Warning, TEXT("%s %f %f %f"), *InName.ToString(), value.X, value.Y, value.Z);
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(OverlayMesh->GetMaterial(0));
	MID->SetVectorParameterValue(InName, value);
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
	// Add pro-rated armor
	int32 TotalArmor = GetArmorAmount();
	if (TotalArmor > 100)
	{
		TotalHealth = TotalHealth + TotalArmor - 50;
	}
	else
	{
		TotalHealth = TotalHealth + TotalArmor / 2;
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
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (Obj && GS && GS->IsMatchInProgress() && !GS->IsMatchIntermission())
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
	else if (!OverrideFellOutOfWorld(DmgType.GetClass()))
	{
		FHitResult FakeHit(this, NULL, GetActorLocation(), GetActorRotation().Vector());
		FUTPointDamageEvent FakeDamageEvent(0, FakeHit, FVector(0, 0, 0), DmgType.GetClass());
		UUTGameplayStatics::UTPlaySound(GetWorld(), CharacterData.GetDefaultObject()->PainSound, this, SRT_All, false, FVector::ZeroVector, Cast<AUTPlayerController>(Controller), NULL, false, SAT_PainSound);
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
	if (bResult && !bIsATest && !bClientUpdating && !bIsTranslocating && TeleportEffect.Num() > 0 && TeleportEffect[0] != NULL)
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
	if (bResult && !bIsATest)
	{
		if (UTCharacterMovement != NULL)
		{
			UTCharacterMovement->NeedsClientAdjustment();
		}
		// trigger update for bots that are moving directly to us, as that move is no longer valid
		for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
		{
			AUTBot* B = Cast<AUTBot>(It->Get());
			if (B != NULL && B->GetMoveTarget().Actor == this)
			{
				B->MoveTimer = -1.0f;
			}
		}
		AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
		if (PS)
		{
			float Dist = (GetActorLocation() - TeleportStart).Size();
			PS->ModifyStatsValue(NAME_TranslocDist, Dist);
		}
	}
	return bResult;
}

bool AUTCharacter::CanBlockTelefrags()
{
	return (GetArmorAmount() > 100);
}

void AUTCharacter::OnOverlapBegin(AActor* OverlappedActor, AActor* OtherActor)
{
	if (Role == ROLE_Authority && OtherActor != this && GetCapsuleComponent()->GetCollisionObjectType() == COLLISION_TELEPORTING_OBJECT) // need to make sure this ISN'T reflexive, only teleporting Pawn should be checking for telefrags
	{
		AUTCharacter* OtherC = Cast<AUTCharacter>(OtherActor);
		if (OtherC != NULL)
		{
			AUTTeamGameMode* TeamGame = GetWorld()->GetAuthGameMode<AUTTeamGameMode>();
			float MinTelefragOverlap = bIsTranslocating ? MinOverlapToTelefrag : 1.f;
			if ((TeamGame == NULL || TeamGame->TeamDamagePct > 0.0f || !GetWorld()->GetGameState<AUTGameState>()->OnSameTeam(OtherC, this)) 
				&& (OtherC->IsRagdoll() || (OtherC->GetActorLocation() - GetActorLocation()).Size2D() < OtherC->GetCapsuleComponent()->GetUnscaledCapsuleRadius() + GetCapsuleComponent()->GetUnscaledCapsuleRadius() - MinTelefragOverlap))
			{
				FUTPointDamageEvent DamageEvent(100000.0f, FHitResult(this, GetCapsuleComponent(), GetActorLocation(), FVector(0.0f, 0.0f, 1.0f)), FVector(0.0f, 0.0f, -1.0f), UUTDmgType_Telefragged::StaticClass());
				if (bIsTranslocating && OtherC->CanBlockTelefrags())
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
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
	const bool bSpectating = PC && PC->PlayerState && PC->PlayerState->bOnlySpectator;
	const bool bTacCom = bSpectating && UTPC && UTPC->bTacComView;
	const bool bOnSameTeam = GS != NULL && GS->OnSameTeam(PC, this);
	const bool bRecentlyRendered = (GetWorld()->TimeSeconds - GetLastRenderTime() < 0.5f);
	const bool bIsViewTarget = (PC->GetViewTarget() == this);

	if (GS && GS->LineUpHelper && GS->LineUpHelper->bIsActive)
	{
 		PostRenderForInGameIntro(PC, Canvas, CameraPosition, CameraDir);
		return;
	}
	if (UTPS == nullptr)
	{
		return;
	}
	FVector WorldPosition = GetMesh()->GetComponentLocation() + FVector(0.f, 0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.25f);
	UTPS->LastPostRenderedLocation = WorldPosition;
	UTPS->bPawnWasPostRendered = true;
	UTPS->PawnPostRenderedTime = GetWorld()->GetTimeSeconds();
	if (UTPC != NULL && (bSpectating || (UTPC && UTPC->UTPlayerState && UTPC->UTPlayerState->bOutOfLives) || !bIsViewTarget) && (bRecentlyRendered || (bOnSameTeam && !bIsViewTarget)) &&
		FVector::DotProduct(CameraDir, (GetActorLocation() - CameraPosition)) > 0.0f && GS != NULL && (UTPC->MyUTHUD == nullptr || !UTPC->MyUTHUD->bShowScores))
	{
		float Dist = (CameraPosition - GetActorLocation()).Size() * FMath::Tan(FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle()*0.5f));
		if ((bOnSameTeam || bSpectating || GS->HasMatchEnded() || GS->IsMatchIntermission()) && (bTacCom || bOnSameTeam || Dist <= (bSpectating ? SpectatorIndicatorMaxDistance : TeamPlayerIndicatorMaxDistance)))
		{
			float TextXL, YL;
			bool bFarAway = (Dist > TeamPlayerIndicatorMaxDistance);
			float ScaleTime = FMath::Min(1.f, 6.f * GetWorld()->DeltaTimeSeconds);
			float Scale = Canvas->ClipX / 1920.f;
			if (bTacCom && !bFarAway && PC->PlayerCameraManager && !bIsViewTarget && (PC->GetViewTarget()->GetAttachmentReplication().AttachParent != this))
			{
				// need to do trace, since taccom guys always rendered
				AUTPlayerCameraManager* CamMgr = Cast<AUTPlayerCameraManager>(PC->PlayerCameraManager);
				if (CamMgr)
				{
					FHitResult Result(1.f);
					CamMgr->CheckCameraSweep(Result, this, CameraPosition, GetActorLocation() + FVector(0.f,0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
					if (Result.bBlockingHit)
					{
						bFarAway = true;
					}
				}
			}
			UFont* ChatFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->ChatFont;
			Canvas->TextSize(ChatFont, PlayerState->PlayerName, TextXL, YL, Scale, Scale);
			float BarWidth, Y;
			Canvas->TextSize(ChatFont, FString("AAAWWW"), BarWidth, Y, Scale, Scale);
			float MaxBarWidth = 2.f*BarWidth;
			float TextScaling = FMath::Min(1.f,  MaxBarWidth/TextXL);
			TextXL *= TextScaling;
			float XL = TextXL;
			FVector ScreenPosition = Canvas->Project(WorldPosition);
			float XPos = ScreenPosition.X - 0.5f*XL;
			float YPos = ScreenPosition.Y - YL; 
			if (XPos < Canvas->ClipX || XPos + XL < 0.0f)
			{
				FLinearColor TeamColor = UTPS->Team ? UTPS->Team->TeamColor : FLinearColor::White;
				float CenterFade = 1.f;
				float Height = 0.75*YL;
				float Border = 2.f*Scale;
				float XDistFromCenter = FMath::Abs(XPos - 0.5f*Canvas->ClipX + 0.5f*XL + Border);
				float YDistFromCenter = FMath::Abs(YPos - 0.5f*Canvas->ClipY + 0.5f*Height + Border);
				float MinBuffer = 0.01f*Canvas->ClipX;
				if ((XDistFromCenter < 0.5*XL + Border + MinBuffer) && (YDistFromCenter < 0.5f*Height + Border + MinBuffer))
				{
					return;
				}
				float PctFromCenter = FVector2D(XDistFromCenter,YDistFromCenter).Size() / Canvas->ClipX;
				CenterFade = CenterFade * FMath::Clamp(15.f*PctFromCenter - 0.1f, 0.f, 1.f);
				if (CenterFade == 0.f)
				{
					return;
				}
				TeamColor.A = 0.2f * CenterFade;
				UTexture* BarTexture = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->HUDAtlas;
				if (bIsInCombat)
				{
					// indicate active combat
					FLinearColor CombatColor(1.f, 0.5f, 0.f, 0.35f * CenterFade);
					Canvas->SetLinearDrawColor(CombatColor);
					float CombatHeight = 0.45f*BarWidth;
					Canvas->DrawTile(BarTexture, ScreenPosition.X - 0.5f*BarWidth, YPos - YL - CombatHeight, BarWidth, CombatHeight, 935.f, 115.f, 65.f, -30.f);
				}

				Canvas->SetLinearDrawColor(TeamColor);
				Canvas->DrawTile(Canvas->DefaultTexture, XPos - Border, YPos - YL - Border, XL + 2.f*Border, Height + 2.f*Border, 0, 0, 1, 1);
				FLinearColor BeaconTextColor = FLinearColor::White;
				BeaconTextColor.A = 0.8f * CenterFade;
				FUTCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos + 0.5f*(XL - TextXL)), FMath::TruncToFloat(Canvas->OrgY + YPos - 1.2f*YL)), FText::FromString(PlayerState->PlayerName), ChatFont, BeaconTextColor, NULL);
				TextItem.Scale = FVector2D(TextScaling*Scale, TextScaling*Scale);
				TextItem.BlendMode = SE_BLEND_Translucent;
				FLinearColor ShadowColor = FLinearColor::Black;
				ShadowColor.A = BeaconTextColor.A;
				TextItem.EnableShadow(ShadowColor);
				TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
				Canvas->DrawItem(TextItem);

				if (bRecentlyRendered && !bFarAway)
				{
					BarWidth *= 0.005f;
					float BackWidth = BarWidth * FMath::Max(100, Health + ArmorAmount);
					XPos = ScreenPosition.X - 0.5f*BackWidth;
					const float BarHeight = 6.f;

					FLinearColor BarColor = FLinearColor::Black;
					BarColor.A = 0.7f * CenterFade;
					Canvas->SetLinearDrawColor(BarColor);
					float BarY = YPos;
					Canvas->DrawTile(BarTexture, XPos, BarY, BackWidth, BarHeight, 185.f, 400.f, 4.f, 4.f);

					BarColor = FLinearColor::Green;
					BarColor.A = 0.7f * CenterFade;
					Canvas->SetLinearDrawColor(BarColor);
					float HealthWidth = BarWidth * Health;
					Canvas->DrawTile(BarTexture, XPos, BarY, HealthWidth, BarHeight, 185.f, 400.f, 4.f, 4.f);
					if (ArmorAmount > 0)
					{
						BarColor = FLinearColor::Yellow;
						BarColor.A = 0.7f * CenterFade;
						Canvas->SetLinearDrawColor(BarColor);
						Canvas->DrawTile(BarTexture, XPos+HealthWidth, BarY, BarWidth * ArmorAmount, BarHeight, 185.f, 400.f, 4.f, 4.f);
					}
				}
			}
		}
	}
}

void AUTCharacter::PostRenderForInGameIntro(APlayerController* PC, UCanvas *Canvas, FVector CameraPosition, FVector CameraDir)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	if (UTPS)
	{
		float TextXL, TextYL, MinTextXL;
		UFont* TinyFont = AUTHUD::StaticClass()->GetDefaultObject<AUTHUD>()->TinyFont;
		FLinearColor TeamColor = UTPS->Team ? UTPS->Team->TeamColor : FLinearColor::Black;
		float Border = 2.f;
		float BarWidth, BarHeight;
		FVector2D Scale(Canvas->ClipX / 1920.f, Canvas->ClipY / 1080.f);

		Canvas->TextSize(TinyFont, PlayerState->PlayerName, TextXL, TextYL, 1.0f, 1.0f);
		Canvas->TextSize(TinyFont, PlayerState->PlayerName, MinTextXL, TextYL, 1.0f, 1.0f);

		TextXL *= Scale.X;
		MinTextXL *= Scale.X;
		TextYL *= Scale.Y;

		BarWidth = Border + ((TextXL > MinTextXL) ? TextXL : MinTextXL);
		BarHeight = Border + (.5*TextYL);
		
		FVector WorldPosition = GetMesh()->GetComponentLocation();
		FVector ScreenPosition = Canvas->Project(WorldPosition + FVector(0.f, 0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.25f));
		float XPos = ScreenPosition.X - 0.5f*BarWidth;
		float YPos = ScreenPosition.Y - BarHeight;

		Canvas->SetLinearDrawColor(TeamColor);

		Canvas->DrawTile(Canvas->DefaultTexture, XPos - Border, YPos - .25*(TextYL), BarWidth + 2.f*Border, BarHeight + 2.f*Border, 0, 0, 1, 1);

		FLinearColor BeaconTextColor = FLinearColor::White;
		BeaconTextColor.A = 0.6f;
		FUTCanvasTextItem TextItem(FVector2D(FMath::TruncToFloat(Canvas->OrgX + XPos), FMath::TruncToFloat(Canvas->OrgY + YPos - (.75*BarHeight))), FText::FromString(PlayerState->PlayerName), TinyFont, BeaconTextColor, NULL);
		TextItem.Scale = Scale;
		TextItem.BlendMode = SE_BLEND_Translucent;
		FLinearColor ShadowColor = FLinearColor::Black;
		ShadowColor.A = BeaconTextColor.A;
		TextItem.EnableShadow(ShadowColor);
		TextItem.FontRenderInfo = Canvas->CreateFontRenderInfo(true, false);
		Canvas->DrawItem(TextItem);
	}
}

void AUTCharacter::SetHatClass(TSubclassOf<AUTHat> HatClass)
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
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bNoFail = true;
		Hat = GetWorld()->SpawnActor<AUTHat>(HatClass, GetActorLocation(), GetActorRotation(), Params);
		if (Hat != nullptr)
		{
			FAttachmentTransformRules HatAttachRules = FAttachmentTransformRules::SnapToTargetIncludingScale;
			HatAttachRules.bWeldSimulatedBodies = true;
			Hat->AttachToComponent(GetMesh(), HatAttachRules, NAME_HatSocket);
			// SnapToTarget doesn't actually snap scale, so do that manually
			Hat->GetRootComponent()->SetWorldScale3D(Cast<USceneComponent>(Hat->GetRootComponent()->GetArchetype())->RelativeScale3D * GetMesh()->GetSocketTransform(NAME_HatSocket).GetScale3D());
			Hat->OnVariantSelected(HatVariant);

			// We may already be invisible
			Hat->SetActorHiddenInGame(bInvisible);

			// If replication of has high score happened before hat replication, locally update it here
			if (bShouldWearLeaderHat)
			{
				LeaderHatStatusChanged();
			}

			// Flatten the hair so it won't show through hats
			GetMesh()->SetMorphTarget(FName(TEXT("HatHair")), 1.0f);
		}
	}
	else
	{
		if (Hat != nullptr)
		{
			Hat->Destroy();
			Hat = nullptr;
		}
	}
}

void AUTCharacter::SetEyewearClass(TSubclassOf<AUTEyewear> EyewearClass)
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
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bNoFail = true;
		Eyewear = GetWorld()->SpawnActor<AUTEyewear>(EyewearClass, GetActorLocation(), GetActorRotation(), Params);
		if (Eyewear != NULL)
		{
			static FName NAME_GlassesSocket(TEXT("GlassesSocket"));
			FAttachmentTransformRules EyewearAttachRules = FAttachmentTransformRules::SnapToTargetIncludingScale;
			EyewearAttachRules.bWeldSimulatedBodies = true;
			Eyewear->AttachToComponent(GetMesh(), EyewearAttachRules, NAME_GlassesSocket);
			// SnapToTarget doesn't actually snap scale, so do that manually
			Eyewear->GetRootComponent()->SetWorldScale3D(Cast<USceneComponent>(Eyewear->GetRootComponent()->GetArchetype())->RelativeScale3D * GetMesh()->GetSocketTransform(NAME_GlassesSocket).GetScale3D());
			Eyewear->OnVariantSelected(EyewearVariant);
			// We may already be invisible
			Eyewear->SetActorHiddenInGame(bInvisible);
		}
	}
	else
	{
		if (Eyewear != nullptr)
		{
			Eyewear->Destroy();
			Eyewear = nullptr;
		}
	}
}

void AUTCharacter::SetHatVariant(int32 NewHatVariant)
{
	HatVariant = NewHatVariant;
	
	if (LeaderHat != nullptr)
	{
		LeaderHat->OnVariantSelected(HatVariant);
	}
	else if (Hat != nullptr)
	{
		Hat->OnVariantSelected(HatVariant);
	}
}

void AUTCharacter::SetEyewearVariant(int32 NewEyewearVariant)
{
	EyewearVariant = NewEyewearVariant;

	if (Eyewear != nullptr)
	{
		Eyewear->OnVariantSelected(EyewearVariant);
	}
}

bool AUTCharacter::IsWearingAnyCosmetic()
{
	if (Hat != nullptr)
	{
		return true;
	}

	if (LeaderHat != nullptr)
	{
		return true;
	}

	if (Eyewear != nullptr)
	{
		return true;
	}

	return false;
}

void AUTCharacter::OnRepCosmeticFlashCount()
{
	if (LeaderHat)
	{
		LeaderHat->OnFlashCountIncremented();
	}
	else if (Hat)
	{
		Hat->OnFlashCountIncremented();
	}
}

void AUTCharacter::OnRepCosmeticSpreeCount()
{
	if (LeaderHat)
	{
		LeaderHat->OnSpreeLevelChanged(CosmeticSpreeCount);
	}
	else if (Hat)
	{
		Hat->OnSpreeLevelChanged(CosmeticSpreeCount);
	}
}

void AUTCharacter::SetEmoteSpeed(float NewEmoteSpeed)
{
	if (CurrentTaunt)
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_SetPlayRate(CurrentTaunt, NewEmoteSpeed);
		}
	}

	if (CurrentFirstPersonTaunt)
	{
		UAnimInstance* FPAnimInstance = FirstPersonMesh->GetAnimInstance();
		if (FPAnimInstance != nullptr)
		{
			FPAnimInstance->Montage_SetPlayRate(CurrentFirstPersonTaunt, NewEmoteSpeed);
		}
	}
}

bool AUTCharacter::IsThirdPersonTaunting() const
{
	if (UTCharacterMovement)
	{
		return UTCharacterMovement->bIsTaunting;
	}

	return false;
}

void AUTCharacter::CascadeGroupTaunt()
{
	// Tell the next character to play the group taunt

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (!GS)
	{
		return;
	}

	TArray<AUTCharacter*> CharacterList;
	int32 CurrentPawnIdx = INDEX_NONE;
	int32 NextPawnIdx = INDEX_NONE;
	bool bStartingPawnFound = false;
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (IsValid(*It))
		{
			AUTCharacter* UTChar = Cast<AUTCharacter>(*It);
			if (UTChar)
			{
				CharacterList.Add(UTChar);
				if (UTChar == this)
				{
					CurrentPawnIdx = CharacterList.Num() - 1;
				}

				if (GS->ScoringPlayerState == UTChar->PlayerState)
				{ 
					bStartingPawnFound = true;
				}
			}			
		}
	}

	if (bStartingPawnFound && CurrentPawnIdx != INDEX_NONE && CharacterList.Num() > 1)
	{
		NextPawnIdx = CurrentPawnIdx + 1;
		if (NextPawnIdx >= CharacterList.Num())
		{
			NextPawnIdx = 0;
		}
		
		if (GS->ScoringPlayerState == CharacterList[NextPawnIdx]->PlayerState)
		{
			// Everyone has played
			return;
		}

		CharacterList[NextPawnIdx]->PlayGroupTaunt(CurrentGroupTauntClass);
	}
}

void AUTCharacter::PlayGroupTaunt(TSubclassOf<AUTGroupTaunt> TauntToPlay)
{
	GetMesh()->bPauseAnims = false;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance != NULL)
	{
		if (CurrentTaunt)
		{
			AnimInstance->Montage_Stop(0.0f, CurrentTaunt);
			CurrentTaunt = nullptr;
		}

		if (AnimInstance->Montage_Play(TauntToPlay->GetDefaultObject<AUTGroupTaunt>()->TauntMontage, 1.0f))
		{
			UTCharacterMovement->bIsTaunting = true;

			CurrentGroupTaunt = TauntToPlay->GetDefaultObject<AUTGroupTaunt>()->TauntMontage;
			CurrentGroupTauntClass = TauntToPlay;

			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(this, &AUTCharacter::OnGroupTauntEnded);
			AnimInstance->Montage_SetEndDelegate(EndDelegate);
		}
	}
}

void AUTCharacter::OnGroupTauntEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (CurrentTauntAudioComponent)
	{
		CurrentTauntAudioComponent->Stop();
		CurrentTauntAudioComponent = nullptr;
	}

	if (Hat)
	{
		Hat->OnWearerEmoteEnded();
	}

	CurrentGroupTaunt = nullptr;
	UTCharacterMovement->bIsTaunting = false;

	// if we're drawing the outline we need the mesh to keep ticking
	if (CustomDepthMesh == NULL || !CustomDepthMesh->IsRegistered())
	{
		GetMesh()->MeshComponentUpdateFlag = GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->MeshComponentUpdateFlag;
	}
}


void AUTCharacter::PlayTauntByClass(TSubclassOf<AUTTaunt> TauntToPlay, float EmoteSpeed)
{
	if (!bFeigningDeath && !IsDead() && TauntToPlay != nullptr && TauntToPlay->GetDefaultObject<AUTTaunt>()->TauntMontage && CurrentGroupTaunt == nullptr)
	{
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
				bool bIncompatibleWeaponForFP = false;
				if (!GetWeapon() || GetWeapon()->HandsAttachSocket == NAME_None)
				{
					bIncompatibleWeaponForFP = true;
				}

				if (TauntToPlay->GetDefaultObject<AUTTaunt>()->FirstPersonTauntMontage == nullptr || bIncompatibleWeaponForFP || !TauntToPlay->GetDefaultObject<AUTTaunt>()->bAllowMovementWithFirstPersonTaunt)
				{
					// This flag is set for 3rd person taunts
					UTCharacterMovement->bIsTaunting = true;
				}
				else if (IsLocallyControlled() && FirstPersonMesh)
				{
					// Don't freeze movement or go to 3rd person camera
					UTCharacterMovement->bIsTaunting = false;

					FirstPersonMesh->bPauseAnims = false;
					FirstPersonMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

					// Play first person taunt
					CurrentFirstPersonTaunt = TauntToPlay->GetDefaultObject<AUTTaunt>()->FirstPersonTauntMontage;
					UAnimInstance* FPAnimInstance = FirstPersonMesh->GetAnimInstance();
					if (FPAnimInstance != NULL)
					{
						FPAnimInstance->Montage_Play(CurrentFirstPersonTaunt, EmoteSpeed);
					}
				}

				CurrentTaunt = TauntToPlay->GetDefaultObject<AUTTaunt>()->TauntMontage;
				TauntCount++;

				// need to make sure server plays the anim and re-enables movement
				GetMesh()->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPoseAndRefreshBones;

				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &AUTCharacter::OnEmoteEnded);
				AnimInstance->Montage_SetEndDelegate(EndDelegate);
			}
		}
	}
}

void AUTCharacter::OnEmoteEnded(UAnimMontage* Montage, bool bInterrupted)
{
	TauntCount--;
	if (TauntCount == 0)
	{
		if (CurrentTauntAudioComponent)
		{
			CurrentTauntAudioComponent->Stop();
			CurrentTauntAudioComponent = nullptr;
		}

		if (Hat)
		{
			Hat->OnWearerEmoteEnded();
		}

		CurrentTaunt = nullptr;
		CurrentFirstPersonTaunt = nullptr;
		UTCharacterMovement->bIsTaunting = false;

		// if we're drawing the outline we need the mesh to keep ticking
		if (CustomDepthMesh == NULL || !CustomDepthMesh->IsRegistered())
		{
			GetMesh()->MeshComponentUpdateFlag = GetClass()->GetDefaultObject<AUTCharacter>()->GetMesh()->MeshComponentUpdateFlag;
		}
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

void AUTCharacter::OnTriggerRallyEffect()
{
	if (bTriggerRallyEffect && (GetNetMode() != NM_DedicatedServer))
	{
		GetMesh()->bPauseAnims = false;
		if (RallyAnimation != nullptr)
		{
			PlayTauntByClass(RallyAnimation, 2.f);
			UTCharacterMovement->bIsTaunting = false;
		}
		SpawnRallyEffectAt(GetActorLocation());
	}
}

void AUTCharacter::SpawnRallyEffectAt(FVector EffectLocation)
{
	TSubclassOf<AUTReplicatedEmitter> PickedEffect = RallyEffect[0];
	int32 TeamNum = GetTeamNum();
	if (TeamNum <RallyEffect.Num() && RallyEffect[TeamNum] != NULL)
	{
		PickedEffect = RallyEffect[TeamNum];
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	GetWorld()->SpawnActor<AUTReplicatedEmitter>(PickedEffect, EffectLocation - FVector(0.f, 0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), GetActorRotation(), Params);
}

void AUTCharacter::SpawnRallyDestinationEffectAt(FVector EffectLocation)
{
	TSubclassOf<AUTReplicatedEmitter> PickedEffect = RallyDestinationEffect[0];
	int32 TeamNum = GetTeamNum();
	if (TeamNum <RallyEffect.Num() && RallyEffect[TeamNum] != NULL)
	{
		PickedEffect = RallyDestinationEffect[TeamNum];
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	GetWorld()->SpawnActor<AUTReplicatedEmitter>(PickedEffect, EffectLocation - FVector(0.f, 0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), GetActorRotation(), Params);
}

float AUTCharacter::GetRemoteViewPitch()
{
	float ClampedPitch = (RemoteViewPitch * 360.f / 255.f);
	ClampedPitch = ClampedPitch > 90.f ? ClampedPitch - 360.f : ClampedPitch;
	return FMath::Clamp<float>(ClampedPitch, -89.f, 89.f);
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
				if ((PredictionTime > 0.f) && (PC->GetViewTarget() != this))
				{
					Cast<UUTCharacterMovement>(GetCharacterMovement())->UTSimulateMovement(PredictionTime);
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
			const FQuat OldRotation = GetActorQuat();
			UTUpdateSimulatedPosition( ReplicatedMovement.Location, ReplicatedMovement.Rotation, ReplicatedMovement.LinearVelocity );

			INetworkPredictionInterface* PredictionInterface = Cast<INetworkPredictionInterface>(GetMovementComponent());
			if (PredictionInterface)
			{
				// todo: SteveP look at me pls
				PredictionInterface->SmoothCorrection(OldLocation, OldRotation, GetActorLocation(), GetActorQuat());
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
	if (bReplicateMovement || GetAttachmentReplication().AttachParent)
	{
		if (GatherUTMovement())
		{
			DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, UTReplicatedMovement, bReplicateMovement);
			DOREPLIFETIME_ACTIVE_OVERRIDE(AActor, ReplicatedMovement, false);
		}
		else
		{
			GatherCurrentMovement();
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

	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, LastTakeHitInfo, GetWorld()->TimeSeconds - LastTakeHitTime < 0.5f);
	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, HeadArmorFlashCount, GetWorld()->TimeSeconds - LastHeadArmorFlashTime < 0.5f);
	DOREPLIFETIME_ACTIVE_OVERRIDE(AUTCharacter, CosmeticFlashCount, GetWorld()->TimeSeconds - LastCosmeticFlashTime < 0.5f);

	// @TODO FIXMESTEVE - just don't want this ever replicated
	DOREPLIFETIME_ACTIVE_OVERRIDE(ACharacter, RemoteViewPitch, false);

	LastTakeHitReplicatedTime = GetWorld()->TimeSeconds;

	if (ChangedPropertyTracker.IsReplay())
	{
		// If this is a replay, we save out certain values we need to runtime to do smooth interpolation
		// We'll be able to look ahead in the replay to have these ahead of time for smoother playback
		FCharacterReplaySample ReplaySample;

		ReplaySample.Location = GetActorLocation();
		ReplaySample.Rotation = GetActorRotation();
		ReplaySample.Velocity = GetVelocity();
		ReplaySample.Acceleration = GetCharacterMovement()->GetCurrentAcceleration();
		ReplaySample.RemoteViewPitch = FRotator::CompressAxisToByte(GetControlRotation().Pitch);

		FBitWriter Writer(0, true);
		Writer << ReplaySample;

		ChangedPropertyTracker.SetExternalData(Writer.GetData(), Writer.GetNumBits());
	}
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
		if (RootComponent->GetAttachParent() != NULL)
		{
			// Networking for attachments assumes the RootComponent of the AttachParent actor. 
			// If that's not the case, we can't update this, as the client wouldn't be able to resolve the Component and would detach as a result.
			if (GetAttachmentReplication().AttachParent != NULL)
			{
				// POLGE TODO: Find an alternative way to set these as this is now private
				/*
				AttachmentReplication.LocationOffset = RootComponent->RelativeLocation;
				AttachmentReplication.RotationOffset = RootComponent->RelativeRotation;
				*/
			

			return false;}
		}
		else
		{
			// @TODO FIXMESTEVE make sure not replicated to owning client!!!
			UTReplicatedMovement.Location = RootComponent->GetComponentLocation();
			UTReplicatedMovement.Rotation = RootComponent->GetComponentRotation();
			UTReplicatedMovement.Rotation.Pitch = GetControlRotation().Pitch;
			UTReplicatedMovement.LinearVelocity = GetVelocity();

			FVector AccelDir = GetCharacterMovement()->GetCurrentAcceleration();
			AccelDir = AccelDir.GetSafeNormal();
			FRotator FacingRot = UTReplicatedMovement.Rotation;
			FacingRot.Pitch = 0.f;
			FVector CurrentDir = FacingRot.Vector();
			float ForwardDot = CurrentDir | AccelDir;

			UTReplicatedMovement.AccelDir = 0;
			if (ForwardDot > 0.5f)
			{
				UTReplicatedMovement.AccelDir |= 1;
			}
			else if (ForwardDot < -0.5f)
			{
				UTReplicatedMovement.AccelDir |= 2;
			}

			FVector SideDir = (CurrentDir ^ FVector(0.f, 0.f, 1.f)).GetSafeNormal();
			float SideDot = AccelDir | SideDir;
			if (SideDot > 0.5f)
			{
				UTReplicatedMovement.AccelDir |= 4;
			}
			else if (SideDot < -0.5f)
			{
				UTReplicatedMovement.AccelDir |= 8;
			}

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

		if (UTCharacterMovement)
		{
			UTCharacterMovement->SetReplicatedAcceleration(UTReplicatedMovement.Rotation, UTReplicatedMovement.AccelDir);
		}
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
		if (RootComponent != NULL)
		{
			// we handle this ourselves, do not use base version
			// why on earth isn't SyncReplicatedPhysicsSimulation() virtual?
			ReplicatedMovement.bRepPhysics = RootComponent->IsSimulatingPhysics();
		}

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
	if (UTCharacterMovement->bIsTaunting || IsFeigningDeath() || (UTCharacterMovement->MovementMode == MOVE_None))
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

bool AUTCharacter::IsFeigningDeath() const
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
			if (Weapon != NULL) // StopFire() could have killed us
			{
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
}

void AUTCharacter::TurnOff()
{
	DisallowWeaponFiring(true);

	if (GetMesh())
	{
		GetMesh()->TickAnimation(1.0f, false);
		GetMesh()->RefreshBoneTransforms();
	}

	Super::TurnOff();
}

//  Don't NetQuantize ClientLoc for verification of perfect synchronization
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

void AUTCharacter::HasHighScoreChanged_Implementation()
{
	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerState);
	bShouldWearLeaderHat = UTPlayerState && UTPlayerState->bHasHighScore;
	LeaderHatStatusChanged_Implementation();
}

void AUTCharacter::OnRep_ShouldWearLeaderHat()
{
	LeaderHatStatusChanged_Implementation();
}

void AUTCharacter::LeaderHatStatusChanged_Implementation()
{
	if (bShouldWearLeaderHat)
	{
		if (LeaderHat == nullptr)
		{
			TSubclassOf<class AUTHatLeader> LeaderHatClass;

			// See if player has selected a leader hat class
			AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
			if (PS && PS->LeaderHatClass)
			{
				LeaderHatClass = PS->LeaderHatClass;
			}

			// Check if equipped hat has a leader variant
			if (LeaderHatClass == nullptr && Hat && Hat->LeaderHatClass && Hat->LeaderHatClass->IsChildOf(AUTHatLeader::StaticClass()))
			{
				LeaderHatClass = Hat->LeaderHatClass;
			}
			
			// Check if game mode is blocking default leader hat, if not use the default
			if (LeaderHatClass == nullptr)
			{
				AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
				if (GS != nullptr && GS->GameModeClass != nullptr)
				{
					TSubclassOf<AUTGameMode> UTGameClass(*GS->GameModeClass);
					if (UTGameClass->GetDefaultObject<AUTGameMode>()->bNoDefaultLeaderHat)
					{
						return;
					}
				}

				LeaderHatClass = DefaultLeaderHatClass;
			}

			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.Instigator = this;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Params.bNoFail = true;
			LeaderHat = GetWorld()->SpawnActor<AUTHatLeader>(LeaderHatClass, GetActorLocation(), GetActorRotation(), Params);
			if (LeaderHat != nullptr)
			{
				LeaderHat->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale, NAME_HatSocket);
				// SnapToTarget doesn't actually snap scale, so do that manually
				LeaderHat->GetRootComponent()->SetWorldScale3D(Cast<USceneComponent>(LeaderHat->GetRootComponent()->GetArchetype())->RelativeScale3D * GetMesh()->GetSocketTransform(NAME_HatSocket).GetScale3D());
				LeaderHat->OnVariantSelected(HatVariant);

				// We may already be invisible
				LeaderHat->SetActorHiddenInGame(bInvisible);

				if (Hat)
				{
					Hat->SetActorHiddenInGame(true);
				}
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

void AUTCharacter::HideCharacter(bool bHideCharacter)
{
	SetActorHiddenInGame(bHideCharacter);
	if (Hat)
	{
		Hat->SetActorHiddenInGame(bHideCharacter);
	}
	if (Eyewear)
	{
		Eyewear->SetActorHiddenInGame(bHideCharacter);
	}
	if (WeaponAttachment)
	{
		WeaponAttachment->SetActorHiddenInGame(bHideCharacter);
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
	if (Eyewear != NULL)
	{
		Eyewear->SetActorHiddenInGame(bInvisible);
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

void AUTCharacter::BehindViewChange(APlayerController* PC, bool bNowBehindView)
{
	if (PC->GetPawn() != this)
	{
		if (!bNowBehindView)
		{
			if (Weapon != NULL && PC->IsLocalPlayerController() && !Weapon->Mesh->IsAttachedTo(CharacterCameraComponent))
			{
				Weapon->AttachToOwner();
			}
		}
		else
		{
			if (Weapon != NULL && (Controller == NULL || !Controller->IsLocalPlayerController()) && Weapon->Mesh->IsAttachedTo(CharacterCameraComponent))
			{
				Weapon->StopFiringEffects();
				Weapon->DetachFromOwner();
			}
		}
	}
	if (bNowBehindView)
	{
		FirstPersonMesh->MeshComponentUpdateFlag = GetClass()->GetDefaultObject<AUTCharacter>()->FirstPersonMesh->MeshComponentUpdateFlag;
	}
	else
	{
		FirstPersonMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose;
		FirstPersonMesh->LastRenderTime = GetWorld()->TimeSeconds;
		FirstPersonMesh->bRecentlyRendered = true;
	}
}
void AUTCharacter::BecomeViewTarget(APlayerController* PC)
{
	Super::BecomeViewTarget(PC);

	AUTPlayerController* UTPC = Cast<AUTPlayerController>(PC);
	if (UTPC != NULL)
	{
		BehindViewChange(UTPC, UTPC->IsBehindView());
	}
	
	//Inform all inventory that the view target has changed
	for (TInventoryIterator<AUTInventory> It(this); It; ++It)
	{
		It->OnViewTargetChange(Cast<AUTPlayerController>(PC));
	}
}
void AUTCharacter::EndViewTarget(APlayerController* PC)
{
	BehindViewChange(PC, true);

	Super::EndViewTarget(PC);
}

bool AUTCharacter::ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor)
{
	return ((Weapon != NULL && Weapon->ProcessConsoleExec(Cmd, Ar, Executor)) || Super::ProcessConsoleExec(Cmd, Ar, Executor));
}

void AUTCharacter::MovementEventUpdated(EMovementEvent MovementEventType, FVector Dir)
{
	MovementEvent.EventType = MovementEventType;
	MovementEvent.EventLocation = GetActorLocation();
	MovementEvent.EventCount++;
	MovementEventDir = Dir;
	if (IsLocallyViewed())
	{
		MovementEventReplicated();
	}

	//Add the event if recording a ghost
	if (GhostComponent->bGhostRecording)
	{
		GhostComponent->GhostMovementEvent(MovementEvent);
	}
}

void AUTCharacter::MovementEventReplicated()
{
	if (Role == ROLE_SimulatedProxy)
	{
		MovementEventDir = UTCharacterMovement->Velocity.GetSafeNormal();
	}
	if (MovementEvent.EventType == EME_Jump)
	{
		PlayJump(MovementEvent.EventLocation, MovementEventDir);
	}
	else if (MovementEvent.EventType == EME_Dodge)
	{
		OnDodge(MovementEvent.EventLocation, MovementEventDir);
	}
	else if (MovementEvent.EventType == EME_WallDodge)
	{
		OnWallDodge(MovementEvent.EventLocation, MovementEventDir);
	}
	else if (MovementEvent.EventType == EME_Slide)
	{
		OnSlide(MovementEvent.EventLocation, MovementEventDir);
	}
}

void AUTCharacter::OnRepWeaponSkin()
{
	UpdateWeaponSkin();
}

void AUTCharacter::UpdateWeaponSkin()
{
	if (WeaponClass == nullptr)
	{
		return;
	}

	UUTWeaponSkin* WeaponSkin = nullptr;
	FString WeaponPathName = WeaponClass->GetPathName();

	for (int32 i = 0; i < WeaponSkins.Num(); i++)
	{
		if (WeaponSkins[i] && WeaponSkins[i]->WeaponType.ToString() == WeaponPathName)
		{
			WeaponSkin = WeaponSkins[i];
			break;
		}
	}

	if (WeaponSkin)
	{
		if (WeaponAttachment && WeaponAttachment->Mesh)
		{
			WeaponAttachment->Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, WeaponSkin->Material);
		}
		if (Weapon)
		{
			Weapon->WeaponSkin = WeaponSkin;

			if (Weapon->Mesh)
			{
				Weapon->Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, WeaponSkin->FPSMaterial);
			}
		}
	}
}

void AUTCharacter::AddVisibilityMask(int32 Channel)
{
	if (Channel < 0 || Channel > 32)
	{
		return;
	}

	VisibilityMask |= (1 << Channel);
}

void AUTCharacter::RemoveVisibilityMask(int32 Channel)
{
	if (Channel < 0 || Channel > 32)
	{
		return;
	}

	VisibilityMask &= ~(1 << Channel);
}


void AUTCharacter::ResetMaxSpeedPctModifier()
{
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	if (UTGameState && UTGameState->bWeightedCharacter)
	{
		AUTCarriedObject* CarriedObject = GetCarriedObject();
		MaxSpeedPctModifier = (CarriedObject) ? CarriedObject->WeightSpeedPctModifier : 1.0f;
		MaxSpeedPctModifier = (MaxSpeedPctModifier == 1.0 && Weapon) ? Weapon->WeightSpeedPctModifier : MaxSpeedPctModifier;
	}
	else
	{
		MaxSpeedPctModifier = 1.0f;
	}
}

void AUTCharacter::BoostSpeedForTime(float SpeedBoostPct, float TimeToBoost)
{
	MaxSpeedPctModifier = SpeedBoostPct;
	GetWorldTimerManager().SetTimer(SpeedBoostTimerHandle, this, &AUTCharacter::ResetMaxSpeedPctModifier, TimeToBoost, false);
}

AActor* AUTCharacter::GetCurrentAimContext()
{
	float MaxRange = 100000.0f; 
	float PawnAim = 0.f;
	float PawnDist = 0.f;
	APawn* BestPawn = Cast<APawn>(UUTGameplayStatics::GetCurrentAimContext(this, 0.9f, MaxRange, APawn::StaticClass(), &PawnAim, &PawnDist));

	float PickupAim = 0.f;
	float PickupDist = 0.f;
	AUTPickup* BestPickup = Cast<AUTPickup>(UUTGameplayStatics::GetCurrentAimContext(this, 0.9f, MaxRange, AUTPickup::StaticClass(), &PickupAim, &PickupAim));
	
	if (BestPickup == nullptr || PawnAim > PickupAim || PawnDist < PickupDist) 
	{
		return BestPawn;
	}
	
	return BestPickup;
}

void AUTCharacter::ClientCheatWalk_Implementation()
{
	SetActorEnableCollision(true);
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bCheatFlying = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
}

void AUTCharacter::ClientCheatFly_Implementation()
{
	SetActorEnableCollision(true);
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bCheatFlying = true;
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
}

void AUTCharacter::ClientCheatGhost_Implementation()
{
	SetActorEnableCollision(false);
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->bCheatFlying = true;
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
}