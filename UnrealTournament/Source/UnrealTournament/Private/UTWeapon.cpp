// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateActive.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeaponStateUnequipping.h"
#include "UTWeaponStateInactive.h"
#include "UTWeaponStateFiringCharged.h"
#include "UTWeaponStateZooming.h"
#include "UTWeaponAttachment.h"
#include "UnrealNetwork.h"
#include "UTHUDWidget.h"
#include "EditorSupportDelegates.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTImpactEffect.h"
#include "UTCharacterMovement.h"
#include "UTWorldSettings.h"
#include "UTPlayerCameraManager.h"
#include "UTHUD.h"
#include "UTGameViewportClient.h"
#include "UTCrosshair.h"

DEFINE_LOG_CATEGORY_STATIC(LogUTWeapon, Log, All);

AUTWeapon::AUTWeapon(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("PickupMesh0")))
{
	AmmoCost.Add(1);
	AmmoCost.Add(1);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;

	bWeaponStay = true;
	bCanThrowWeapon = true;

	Ammo = 20;
	MaxAmmo = 50;

	BringUpTime = 0.37f;
	PutDownTime = 0.3f;
	WeaponBobScaling = 1.f;
	FiringViewKickback = -20.f;
	bNetDelayedShot = false;

	bFPFireFromCenter = true;
	bFPIgnoreInstantHitFireOffset = true;
	FireOffset = FVector(75.0f, 0.0f, 0.0f);
	FriendlyMomentumScaling = 1.f;
	FireEffectInterval = 1;
	FireEffectCount = 0;

	InactiveState = ObjectInitializer.CreateDefaultSubobject<UUTWeaponStateInactive>(this, TEXT("StateInactive"));
	ActiveState = ObjectInitializer.CreateDefaultSubobject<UUTWeaponStateActive>(this, TEXT("StateActive"));
	EquippingState = ObjectInitializer.CreateDefaultSubobject<UUTWeaponStateEquipping>(this, TEXT("StateEquipping"));
	UnequippingStateDefault = ObjectInitializer.CreateDefaultSubobject<UUTWeaponStateUnequipping>(this, TEXT("StateUnequipping"));
	UnequippingState = UnequippingStateDefault;

	Mesh = ObjectInitializer.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Mesh1P"));
	Mesh->SetOnlyOwnerSee(true);
	Mesh->AttachParent = RootComponent;
	Mesh->bSelfShadowOnly = true;
	Mesh->bReceivesDecals = false;
	Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	FirstPMeshOffset = FVector(0.f);
	FirstPMeshRotation = FRotator(0.f, 0.f, 0.f);

	for (int32 i = 0; i < 2; i++)
	{
		UUTWeaponStateFiring* NewState = ObjectInitializer.CreateDefaultSubobject<UUTWeaponStateFiring, UUTWeaponStateFiring>(this, FName(*FString::Printf(TEXT("FiringState%i"), i)), false);
		if (NewState != NULL)
		{
			FiringState.Add(NewState);
			FireInterval.Add(1.0f);
		}
	}

	RotChgSpeed = 3.f;
	ReturnChgSpeed = 3.f;
	MaxYawLag = 4.4f;
	MaxPitchLag = 3.3f;
	FOVOffset = FVector(1.f);
	bProceduralLagRotation = true;

	// default icon texture
	static ConstructorHelpers::FObjectFinder<UTexture> WeaponTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseB.UI_HUD_BaseB'"));
	HUDIcon.Texture = WeaponTexture.Object;

	BaseAISelectRating = 0.55f;
	DisplayName = NSLOCTEXT("PickupMessage", "WeaponPickedUp", "Weapon");
	IconColor = FLinearColor::White;
	bShowPowerupTimer = false;

}

void AUTWeapon::PostInitProperties()
{
	Super::PostInitProperties();

	WeaponBarScale = 0.0f;

	if (DisplayName.IsEmpty() || (GetClass() != AUTWeapon::StaticClass() && DisplayName.EqualTo(GetClass()->GetSuperClass()->GetDefaultObject<AUTWeapon>()->DisplayName) && GetClass()->GetSuperClass()->GetDefaultObject<AUTWeapon>()->DisplayName.EqualTo(FText::FromName(GetClass()->GetSuperClass()->GetFName()))))
	{
		DisplayName = FText::FromName(GetClass()->GetFName());
	}
}

UMeshComponent* AUTWeapon::GetPickupMeshTemplate_Implementation(FVector& OverrideScale) const
{
	if (AttachmentType != NULL)
	{
		OverrideScale = AttachmentType.GetDefaultObject()->PickupScaleOverride;
		return AttachmentType.GetDefaultObject()->Mesh;
	}
	else
	{
		return Super::GetPickupMeshTemplate_Implementation(OverrideScale);
	}
}

void AUTWeapon::InstanceMuzzleFlashArray(AActor* Weap, TArray<UParticleSystemComponent*>& MFArray)
{
	TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
	UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(Weap->GetClass(), ParentBPClassStack);
	for (int32 i = ParentBPClassStack.Num() - 1; i >= 0; i--)
	{
		const UBlueprintGeneratedClass* CurrentBPGClass = ParentBPClassStack[i];
		if (CurrentBPGClass->SimpleConstructionScript)
		{
			TArray<USCS_Node*> ConstructionNodes = CurrentBPGClass->SimpleConstructionScript->GetAllNodes();
			for (int32 j = 0; j < ConstructionNodes.Num(); j++)
			{
				for (int32 k = 0; k < MFArray.Num(); k++)
				{
					if (Cast<UParticleSystemComponent>(ConstructionNodes[j]->ComponentTemplate) == MFArray[k])
					{
						MFArray[k] = Cast<UParticleSystemComponent>((UObject*)FindObjectWithOuter(Weap, ConstructionNodes[j]->ComponentTemplate->GetClass(), ConstructionNodes[j]->VariableName));
					}
				}
			}
		}
	}
}

void AUTWeapon::BeginPlay()
{
	Super::BeginPlay();

	InstanceMuzzleFlashArray(this, MuzzleFlash);
	// sanity check some settings
	for (int32 i = 0; i < MuzzleFlash.Num(); i++)
	{
		if (MuzzleFlash[i] != NULL)
		{
			if (RootComponent == NULL && MuzzleFlash[i]->IsRegistered())
			{
				MuzzleFlash[i]->DeactivateSystem();
				MuzzleFlash[i]->KillParticlesForced();
				MuzzleFlash[i]->UnregisterComponent(); // SCS components were registered without our permission
				MuzzleFlash[i]->bWasActive = false;
			}
			MuzzleFlash[i]->bAutoActivate = false;
			MuzzleFlash[i]->SecondsBeforeInactive = 0.0f;
			MuzzleFlash[i]->SetOnlyOwnerSee(false); // we handle this in AUTPlayerController::UpdateHiddenComponents() instead
		}
	}

	// might have already been activated if at startup, see ClientGivenTo_Internal()
	if (CurrentState == NULL)
	{
		GotoState(InactiveState);
	}
	checkSlow(CurrentState != NULL);
}

void AUTWeapon::GotoState(UUTWeaponState* NewState)
{
	if (NewState == NULL || !NewState->IsIn(this))
	{
		UE_LOG(UT, Warning, TEXT("Attempt to send %s to invalid state %s"), *GetName(), *GetFullNameSafe(NewState));
	}
	else if (ensureMsgf(UTOwner != NULL || NewState == InactiveState, TEXT("Attempt to send %s to state %s while not owned"), *GetName(), *GetNameSafe(NewState)))
	{
		if (CurrentState != NewState)
		{
			UUTWeaponState* PrevState = CurrentState;
			if (CurrentState != NULL)
			{
				CurrentState->EndState(); // NOTE: may trigger another GotoState() call
			}
			if (CurrentState == PrevState)
			{
				CurrentState = NewState;
				CurrentState->BeginState(PrevState); // NOTE: may trigger another GotoState() call
				StateChanged();
			}
		}
	}
}

void AUTWeapon::GotoFireMode(uint8 NewFireMode)
{
	if (FiringState.IsValidIndex(NewFireMode))
	{
		CurrentFireMode = NewFireMode;
		GotoState(FiringState[NewFireMode]);
	}
}

void AUTWeapon::GotoEquippingState(float OverflowTime)
{
	GotoState(EquippingState);
	if (CurrentState == EquippingState)
	{
		EquippingState->StartEquip(OverflowTime);
	}
}

void AUTWeapon::GivenTo(AUTCharacter* NewOwner, bool bAutoActivate)
{
	Super::GivenTo(NewOwner, bAutoActivate);

	// if character has ammo on it, transfer to weapon
	for (int32 i = 0; i < NewOwner->SavedAmmo.Num(); i++)
	{
		if (NewOwner->SavedAmmo[i].Type == GetClass())
		{
			AddAmmo(NewOwner->SavedAmmo[i].Amount);
			NewOwner->SavedAmmo.RemoveAt(i);
			break;
		}
	}
}

void AUTWeapon::ClientGivenTo_Internal(bool bAutoActivate)
{
	if (bMustBeHolstered && UTOwner && HasAnyAmmo())
	{
		AttachToHolster();
	}

	// make sure we initialized our state; this can be triggered if the weapon is spawned at game startup, since BeginPlay() will be deferred
	if (CurrentState == NULL)
	{
		GotoState(InactiveState);
	}

	Super::ClientGivenTo_Internal(bAutoActivate);

	// Grab our switch priority
	AUTPlayerController *UTPC = Cast<AUTPlayerController>(UTOwner->Controller);
	if (UTPC != NULL)
	{
		AutoSwitchPriority = UTPC->GetWeaponAutoSwitchPriority(GetNameSafe(this), AutoSwitchPriority);
	}

	// assign GroupSlot if required
	int32 MaxGroupSlot = 0;
	bool bDuplicateSlot = false;
	for (TInventoryIterator<AUTWeapon> It(UTOwner); It; ++It)
	{
		if (*It != this && It->Group == Group)
		{
			MaxGroupSlot = FMath::Max<int32>(MaxGroupSlot, It->GroupSlot);
			bDuplicateSlot = bDuplicateSlot || (GroupSlot == It->GroupSlot);
		}
	}
	if (bDuplicateSlot)
	{
		GroupSlot = MaxGroupSlot + 1;
	}

	if (bAutoActivate && UTPC != NULL)
	{
		UTPC->CheckAutoWeaponSwitch(this);
	}
}

void AUTWeapon::DropFrom(const FVector& StartLocation, const FVector& TossVelocity)
{
	if (UTOwner && bMustBeHolstered)
	{
		DetachFromHolster();
	}

	if (!HasAnyAmmo())
	{
		Destroy();
	}
	else
	{
		Super::DropFrom(StartLocation, TossVelocity);
	}
}

void AUTWeapon::Removed()
{
	GotoState(InactiveState);
	DetachFromOwner();
	if (bMustBeHolstered)
	{
		DetachFromHolster();
	}

	Super::Removed();
}

void AUTWeapon::ClientRemoved_Implementation()
{
	GotoState(InactiveState);
	if (Role < ROLE_Authority) // already happened on authority in Removed()
	{
		DetachFromOwner();
		if (bMustBeHolstered)
		{
			DetachFromHolster();
		}
	}

	AUTCharacter* OldOwner = UTOwner;

	Super::ClientRemoved_Implementation();

	if (OldOwner != NULL && (OldOwner->GetWeapon() == this || OldOwner->GetPendingWeapon() == this))
	{
		OldOwner->ClientWeaponLost(this);
	}
}

bool AUTWeapon::FollowsInList(AUTWeapon* OtherWeapon, bool bUseClassicGroups)
{
	// return true if this weapon is after OtherWeapon in the weapon list
	if (!OtherWeapon)
	{
		return true;
	}
	// if same group, order by slot, else order by group number
	if (bUseClassicGroups)
	{
		return (ClassicGroup == OtherWeapon->ClassicGroup) ? (GroupSlot > OtherWeapon->GroupSlot) : (ClassicGroup > OtherWeapon->ClassicGroup);
	}
	else
	{
		return (Group == OtherWeapon->Group) ? (GroupSlot > OtherWeapon->GroupSlot) : (Group > OtherWeapon->Group);
	}
}

void AUTWeapon::StartFire(uint8 FireModeNum)
{
	if (!UTOwner->IsFiringDisabled())
	{
		bool bClientFired = BeginFiringSequence(FireModeNum, false);
		if (Role < ROLE_Authority)
		{
			ServerStartFire(FireModeNum, bClientFired); 
		}
	}
}

void AUTWeapon::ServerStartFire_Implementation(uint8 FireModeNum, bool bClientFired)
{
	if (!UTOwner->IsFiringDisabled())
	{
		BeginFiringSequence(FireModeNum, bClientFired);
	}
}

bool AUTWeapon::ServerStartFire_Validate(uint8 FireModeNum, bool bClientFired)
{
	return true;
}

bool AUTWeapon::WillSpawnShot(float DeltaTime)
{
	return (CurrentState != NULL) && CanFireAgain() && CurrentState->WillSpawnShot(DeltaTime);
}

bool AUTWeapon::BeginFiringSequence(uint8 FireModeNum, bool bClientFired)
{
	if (UTOwner)
	{
		UTOwner->SetPendingFire(FireModeNum, true);
		if (FiringState.IsValidIndex(FireModeNum) && CurrentState != EquippingState && CurrentState != UnequippingState)
		{
			FiringState[FireModeNum]->PendingFireStarted();
		}
		bool bResult = CurrentState->BeginFiringSequence(FireModeNum, bClientFired);
		if (CurrentState->IsFiring() && CurrentFireMode != FireModeNum)
		{
			OnMultiPress(FireModeNum);
		}
		return bResult;
	}
	return false;
}

void AUTWeapon::StopFire(uint8 FireModeNum)
{
	EndFiringSequence(FireModeNum);
	if (Role < ROLE_Authority)
	{
		ServerStopFire(FireModeNum);
	}
}

void AUTWeapon::ServerStopFire_Implementation(uint8 FireModeNum)
{
	EndFiringSequence(FireModeNum);
}

bool AUTWeapon::ServerStopFire_Validate(uint8 FireModeNum)
{
	return true;
}

void AUTWeapon::EndFiringSequence(uint8 FireModeNum)
{
	if (UTOwner)
	{
		UTOwner->SetPendingFire(FireModeNum, false);
	}
	if (FiringState.IsValidIndex(FireModeNum) && CurrentState != EquippingState && CurrentState != UnequippingState)
	{
		FiringState[FireModeNum]->PendingFireStopped();
	}
	CurrentState->EndFiringSequence(FireModeNum);
}

void AUTWeapon::BringUp(float OverflowTime)
{
	AttachToOwner();
	OnBringUp();
	CurrentState->BringUp(OverflowTime);
}

float AUTWeapon::GetPutDownTime()
{
	return PutDownTime;
}

float AUTWeapon::GetBringUpTime()
{
	return BringUpTime;
}

bool AUTWeapon::PutDown()
{
	if (eventPreventPutDown())
	{
		return false;
	}
	else
	{
		SetZoomState(EZoomState::EZS_NotZoomed);
		CurrentState->PutDown();
		return true;
	}
}

void AUTWeapon::UnEquip()
{

	GotoState(UnequippingState);
}

void AUTWeapon::AttachToHolster()
{
	UTOwner->SetHolsteredWeaponAttachmentClass(AttachmentType);
	UTOwner->UpdateHolsteredWeaponAttachment();
}

void AUTWeapon::DetachFromHolster()
{
	if (UTOwner != NULL)
	{
		UTOwner->SetHolsteredWeaponAttachmentClass(NULL);
		UTOwner->UpdateHolsteredWeaponAttachment();
	}
}

void AUTWeapon::AttachToOwner_Implementation()
{
	if (UTOwner == NULL)
	{
		return;
	}

	if (bMustBeHolstered)
	{
		// detach from holster if becoming held
		DetachFromHolster();
	}

	// attach
	if (Mesh != NULL && Mesh->SkeletalMesh != NULL)
	{
		UpdateWeaponHand();
		Mesh->AttachTo(UTOwner->FirstPersonMesh, (GetWeaponHand() != HAND_Hidden) ? HandsAttachSocket : NAME_None);
		if (ShouldPlay1PVisuals())
		{
			Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose; // needed for anims to be ticked even if weapon is not currently displayed, e.g. sniper zoom
			Mesh->LastRenderTime = GetWorld()->TimeSeconds;
			Mesh->bRecentlyRendered = true;
			if (OverlayMesh != NULL)
			{
				OverlayMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::AlwaysTickPose;
				OverlayMesh->LastRenderTime = GetWorld()->TimeSeconds;
				OverlayMesh->bRecentlyRendered = true;
			}
		}
	}
	// register components now
	Super::RegisterAllComponents();
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateOverlays();
		SetSkin(UTOwner->GetSkin());
	}
}

void AUTWeapon::UpdateWeaponHand()
{
	if (Mesh != NULL && UTOwner != NULL)
	{
		FirstPMeshOffset = FVector::ZeroVector;
		FirstPMeshRotation = FRotator::ZeroRotator;

		if (MuzzleFlashDefaultTransforms.Num() == 0)
		{
			for (UParticleSystemComponent* PSC : MuzzleFlash)
			{
				MuzzleFlashDefaultTransforms.Add((PSC == NULL) ? FTransform::Identity : PSC->GetRelativeTransform());
				MuzzleFlashSocketNames.Add((PSC == NULL) ? NAME_None : PSC->AttachSocketName);
			}
		}
		else
		{
			for (int32 i = 0; i < FMath::Min3<int32>(MuzzleFlash.Num(), MuzzleFlashDefaultTransforms.Num(), MuzzleFlashSocketNames.Num()); i++)
			{
				if (MuzzleFlash[i] != NULL)
				{
					MuzzleFlash[i]->AttachSocketName = MuzzleFlashSocketNames[i];
					MuzzleFlash[i]->SetRelativeTransform(MuzzleFlashDefaultTransforms[i]);
				}
			}
		}

		Mesh->AttachSocketName = HandsAttachSocket;
		if (HandsAttachSocket == NAME_None)
		{
			UTOwner->FirstPersonMesh->SetRelativeTransform(FTransform::Identity);
		}
		else
		{
			USkeletalMeshComponent* DefaultHands = UTOwner->GetClass()->GetDefaultObject<AUTCharacter>()->FirstPersonMesh;
			UTOwner->FirstPersonMesh->RelativeLocation = DefaultHands->RelativeLocation;
			UTOwner->FirstPersonMesh->RelativeRotation = DefaultHands->RelativeRotation;
			UTOwner->FirstPersonMesh->RelativeScale3D = DefaultHands->RelativeScale3D;
			UTOwner->FirstPersonMesh->UpdateComponentToWorld();
		}

		USkeletalMeshComponent* AdjustMesh = (HandsAttachSocket != NAME_None) ? UTOwner->FirstPersonMesh : Mesh;
		USkeletalMeshComponent* AdjustMeshArchetype = Cast<USkeletalMeshComponent>(AdjustMesh->GetArchetype());

		switch (GetWeaponHand())
		{
			case HAND_Center:
				// TODO: not implemented, fallthrough
				UE_LOG(UT, Warning, TEXT("HAND_Center is not implemented yet!"));
			case HAND_Right:
				AdjustMesh->SetRelativeLocationAndRotation(AdjustMeshArchetype->RelativeLocation, AdjustMeshArchetype->RelativeRotation);
				break;
			case HAND_Left:
			{
				// TODO: should probably mirror, but mirroring breaks sockets at the moment (engine bug)
				AdjustMesh->SetRelativeLocation(AdjustMeshArchetype->RelativeLocation * FVector(1.0f, -1.0f, 1.0f));
				FRotator AdjustedRotation = (FRotationMatrix(AdjustMeshArchetype->RelativeRotation) * FScaleMatrix(FVector(1.0f, 1.0f, -1.0f))).Rotator();
				AdjustMesh->SetRelativeRotation(AdjustedRotation);
				break;
			}
			case HAND_Hidden:
			{
				AdjustMesh->SetRelativeLocationAndRotation(FVector(-50.0f, 0.0f, -50.0f), FRotator::ZeroRotator);
				if (AdjustMesh != Mesh)
				{
					Mesh->AttachSocketName = NAME_None;
					Mesh->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
				}
				for (int32 i = 0; i < MuzzleFlash.Num() && i < MuzzleFlashDefaultTransforms.Num(); i++)
				{
					if (MuzzleFlash[i] != NULL)
					{
						MuzzleFlash[i]->AttachSocketName = NAME_None;
						MuzzleFlash[i]->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
					}
				}
				break;
			}
		}
	}
}

EWeaponHand AUTWeapon::GetWeaponHand() const
{
	if (UTOwner == NULL && Role == ROLE_Authority)
	{
		return HAND_Right;
	}
	else
	{
		AUTPlayerController* Viewer = NULL;
		if (UTOwner != NULL)
		{
			if (Role == ROLE_Authority)
			{
				Viewer = Cast<AUTPlayerController>(UTOwner->Controller);
			}
			if (Viewer == NULL)
			{
				Viewer = UTOwner->GetLocalViewer();
				if (Viewer == NULL && UTOwner->Controller != NULL && UTOwner->Controller->IsLocalPlayerController())
				{
					// this can happen during initial replication; Controller might be set but ViewTarget not
					Viewer = Cast<AUTPlayerController>(UTOwner->Controller);
				}
			}
		}
		return (Viewer != NULL) ? Viewer->GetWeaponHand() : HAND_Right;
	}
}

void AUTWeapon::DetachFromOwner_Implementation()
{
	StopFiringEffects();
	// make sure particle system really stops NOW since we're going to unregister it
	for (int32 i = 0; i < MuzzleFlash.Num(); i++)
	{
		if (MuzzleFlash[i] != NULL)
		{
			UParticleSystem* SavedTemplate = MuzzleFlash[i]->Template;
			MuzzleFlash[i]->DeactivateSystem();
			MuzzleFlash[i]->KillParticlesForced();
			// FIXME: KillParticlesForced() doesn't kill particles immediately for GPU particles, but the below does...
			MuzzleFlash[i]->SetTemplate(NULL);
			MuzzleFlash[i]->SetTemplate(SavedTemplate);
		}
	}
	if (Mesh != NULL && Mesh->SkeletalMesh != NULL)
	{
		Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
		Mesh->DetachFromParent();
		if (OverlayMesh != NULL)
		{
			OverlayMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
		}
	}
	// unregister components so they go away
	UnregisterAllComponents();

	if (bMustBeHolstered && HasAnyAmmo() && UTOwner && !UTOwner->IsDead() && !IsPendingKillPending())
	{
		AttachToHolster();
	}
}

bool AUTWeapon::IsChargedFireMode(uint8 TestMode) const
{
	return FiringState.IsValidIndex(TestMode) && Cast<UUTWeaponStateFiringCharged>(FiringState[TestMode]) != NULL;
}

bool AUTWeapon::ShouldPlay1PVisuals() const
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	else
	{
		// note we can't check Mesh->LastRenderTime here because of the hidden weapon setting!
		return UTOwner && UTOwner->GetLocalViewer() && !UTOwner->GetLocalViewer()->IsBehindView();
	}
}

void AUTWeapon::PlayWeaponAnim(UAnimMontage* WeaponAnim, UAnimMontage* HandsAnim, float RateOverride)
{
	if (RateOverride <= 0.0f)
	{
		RateOverride = UTOwner->GetFireRateMultiplier();
	}
	if (UTOwner != NULL)
	{
		if (WeaponAnim != NULL)
		{
			UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(WeaponAnim, RateOverride);
			}
		}
		if (HandsAnim != NULL)
		{
			UAnimInstance* AnimInstance = UTOwner->FirstPersonMesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(HandsAnim, RateOverride);
			}
		}
	}
}

UAnimMontage* AUTWeapon::GetFiringAnim(uint8 FireMode, bool bOnHands) const
{
	const TArray<UAnimMontage*>& AnimArray = bOnHands ? FireAnimationHands : FireAnimation;
	return (AnimArray.IsValidIndex(CurrentFireMode) ? AnimArray[CurrentFireMode] : NULL);
}

void AUTWeapon::PlayFiringEffects()
{
	if (UTOwner != NULL)
	{
		uint8 EffectFiringMode = (Role == ROLE_Authority || UTOwner->Controller != NULL) ? CurrentFireMode : UTOwner->FireMode;

		// try and play the sound if specified
		if (FireSound.IsValidIndex(EffectFiringMode) && FireSound[EffectFiringMode] != NULL)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), FireSound[EffectFiringMode], UTOwner, SRT_AllButOwner);
		}

		if (ShouldPlay1PVisuals() && GetWeaponHand() != HAND_Hidden)
		{
			UTOwner->TargetEyeOffset.X = FiringViewKickback;
			// try and play a firing animation if specified
			PlayWeaponAnim(GetFiringAnim(EffectFiringMode, false), GetFiringAnim(EffectFiringMode, true));

			// muzzle flash
			if (MuzzleFlash.IsValidIndex(EffectFiringMode) && MuzzleFlash[EffectFiringMode] != NULL && MuzzleFlash[EffectFiringMode]->Template != NULL)
			{
				// if we detect a looping particle system, then don't reactivate it
				if (!MuzzleFlash[EffectFiringMode]->bIsActive || MuzzleFlash[EffectFiringMode]->bSuppressSpawning || !IsLoopingParticleSystem(MuzzleFlash[EffectFiringMode]->Template))
				{
					MuzzleFlash[EffectFiringMode]->ActivateSystem();
				}
			}
		}
	}
}

void AUTWeapon::StopFiringEffects()
{
	for (UParticleSystemComponent* MF : MuzzleFlash)
	{
		if (MF != NULL)
		{
			MF->DeactivateSystem();
		}
	}
}

FHitResult AUTWeapon::GetImpactEffectHit(APawn* Shooter, const FVector& StartLoc, const FVector& TargetLoc)
{
	// trace for precise hit location and hit normal
	FHitResult Hit;
	FVector TargetToGun = (StartLoc - TargetLoc).GetSafeNormal();
	if (Shooter->GetWorld()->LineTraceSingleByChannel(Hit, TargetLoc + TargetToGun * 32.0f, TargetLoc - TargetToGun * 32.0f, COLLISION_TRACE_WEAPON, FCollisionQueryParams(FName(TEXT("ImpactEffect")), true, Shooter)))
	{
		return Hit;
	}
	else
	{
		return FHitResult(NULL, NULL, TargetLoc, TargetToGun);
	}
}

void AUTWeapon::GetImpactSpawnPosition(const FVector& TargetLoc, FVector& SpawnLocation, FRotator& SpawnRotation)
{
	SpawnLocation = (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL) ? MuzzleFlash[CurrentFireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetControlRotation().RotateVector(FireOffset);
	SpawnRotation = (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL) ? MuzzleFlash[CurrentFireMode]->GetComponentRotation() : (TargetLoc - SpawnLocation).Rotation();
}

bool AUTWeapon::CancelImpactEffect(const FHitResult& ImpactHit)
{
	return ImpactHit.Actor.IsValid() && (Cast<AUTCharacter>(ImpactHit.Actor.Get()) || Cast<AUTProjectile>(ImpactHit.Actor.Get()));
}

void AUTWeapon::PlayImpactEffects(const FVector& TargetLoc, uint8 FireMode, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		// fire effects
		static FName NAME_HitLocation(TEXT("HitLocation"));
		static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));
		FireEffectCount++;
		if (FireEffect.IsValidIndex(FireMode) && (FireEffect[FireMode] != NULL) && (FireEffectCount >= FireEffectInterval))
		{
			FVector AdjustedSpawnLocation = SpawnLocation;
			// panini project the location, if necessary
			if (Mesh != NULL)
			{
				for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
				{
					if (It->PlayerController != NULL && It->PlayerController->GetViewTarget() == UTOwner)
					{
						UUTGameViewportClient* UTViewport = Cast<UUTGameViewportClient>(It->ViewportClient);
						if (UTViewport != NULL)
						{
							AdjustedSpawnLocation = UTViewport->PaniniProjectLocationForPlayer(*It, SpawnLocation, Mesh->GetMaterial(0));
							break;
						}
					}
				}
			}
			FireEffectCount = 0;
			UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[FireMode], AdjustedSpawnLocation, SpawnRotation, true);
			PSC->SetVectorParameter(NAME_HitLocation, TargetLoc);
			PSC->SetVectorParameter(NAME_LocalHitLocation, PSC->ComponentToWorld.InverseTransformPosition(TargetLoc));
			ModifyFireEffect(PSC);
		}
		// perhaps the muzzle flash also contains hit effect (constant beam, etc) so set the parameter on it instead
		else if (MuzzleFlash.IsValidIndex(FireMode) && MuzzleFlash[FireMode] != NULL)
		{
			MuzzleFlash[FireMode]->SetVectorParameter(NAME_HitLocation, TargetLoc);
			MuzzleFlash[FireMode]->SetVectorParameter(NAME_LocalHitLocation, MuzzleFlash[FireMode]->ComponentToWorld.InverseTransformPositionNoScale(TargetLoc));
		}

		// Always spawn effects instigated by local player unless beyond cull distance
		if ((TargetLoc - LastImpactEffectLocation).Size() >= ImpactEffectSkipDistance || GetWorld()->TimeSeconds - LastImpactEffectTime >= MaxImpactEffectSkipTime)
		{
			if (ImpactEffect.IsValidIndex(FireMode) && ImpactEffect[FireMode] != NULL)
			{
				FHitResult ImpactHit = GetImpactEffectHit(UTOwner, SpawnLocation, TargetLoc);
				if (ImpactHit.Component.IsValid() && !CancelImpactEffect(ImpactHit))
				{
					ImpactEffect[FireMode].GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(ImpactHit.Normal.Rotation(), ImpactHit.Location), ImpactHit.Component.Get(), NULL, UTOwner->Controller);
				}
			}
			LastImpactEffectLocation = TargetLoc;
			LastImpactEffectTime = GetWorld()->TimeSeconds;
		}
	}
}

void AUTWeapon::DeactivateSpawnProtection()
{
	if (UTOwner)
	{
		UTOwner->DeactivateSpawnProtection();
	}
}

void AUTWeapon::FireShot()
{
	UTOwner->DeactivateSpawnProtection();
	ConsumeAmmo(CurrentFireMode);

	if (!FireShotOverride() && GetUTOwner() != NULL) // script event may kill user
	{
		if (ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL)
		{
			FireProjectile();
		}
		else if (InstantHitInfo.IsValidIndex(CurrentFireMode) && InstantHitInfo[CurrentFireMode].DamageType != NULL)
		{
			FireInstantHit();
		}
		PlayFiringEffects();
	}
	if (GetUTOwner() != NULL)
	{
		GetUTOwner()->InventoryEvent(InventoryEventName::FiredWeapon);
	}
}

bool AUTWeapon::IsFiring() const
{
	return CurrentState->IsFiring();
}

void AUTWeapon::AddAmmo(int32 Amount)
{
	if (Role == ROLE_Authority)
	{
		Ammo = FMath::Clamp<int32>(Ammo + Amount, 0, MaxAmmo);

		// trigger weapon switch if necessary
		if (UTOwner != NULL && UTOwner->IsLocallyControlled())
		{
			OnRep_Ammo();
		}
	}
}

void AUTWeapon::OnRep_Ammo()
{
	if (UTOwner != NULL && UTOwner->GetPendingWeapon() == NULL && !HasAnyAmmo())
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(UTOwner->Controller);
		if (PC != NULL)
		{
			//UE_LOG(UT, Warning, TEXT("********** %s ran out of ammo for %s"), *GetName(), *PC->GetHumanReadableName());
			PC->SwitchToBestWeapon();
		}
		else
		{
			AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
			if (B != NULL)
			{
				B->SwitchToBestWeapon();
			}
		}
	}
}

void AUTWeapon::OnRep_AttachmentType()
{
	if (UTOwner)
	{
		GetUTOwner()->UpdateWeaponAttachment();
	}
	else
	{	
		AttachmentType = NULL;
	}
}

void AUTWeapon::ConsumeAmmo(uint8 FireModeNum)
{
	if (Role == ROLE_Authority )
	{
		AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (AmmoCost.IsValidIndex(FireModeNum) && (!GameMode || GameMode->bAmmoIsLimited || (Ammo > 9)))
		{
			AddAmmo(-AmmoCost[FireModeNum]);
		}
		else if (!AmmoCost.IsValidIndex(FireModeNum))
		{
			UE_LOG(UT, Warning, TEXT("Invalid fire mode %i in %s::ConsumeAmmo()"), int32(FireModeNum), *GetName());
		}
	}
}

bool AUTWeapon::HasAmmo(uint8 FireModeNum)
{
	return (AmmoCost.IsValidIndex(FireModeNum) && Ammo >= AmmoCost[FireModeNum]);
}

bool AUTWeapon::NeedsAmmoDisplay() const
{
	for (int32 i = GetNumFireModes() - 1; i >= 0; i--)
	{
		if (AmmoCost[i] > 0)
		{
			return true;
		}
	}
	return false;
}

bool AUTWeapon::HasAnyAmmo()
{
	bool bHadCost = false;

	// only consider zero cost firemodes as having ammo if they all have no cost
	// the assumption here is that for most weapons with an ammo-using firemode,
	// any that don't use ammo are support firemodes that can't function effectively without the other one
	for (int32 i = GetNumFireModes() - 1; i >= 0; i--)
	{
		if (AmmoCost[i] > 0)
		{
			bHadCost = true;
			if (HasAmmo(i))
			{
				return true;
			}
		}
	}
	return !bHadCost;
}

FVector AUTWeapon::GetFireStartLoc(uint8 FireMode)
{
	// default to current firemode
	if (FireMode == 255)
	{
		FireMode = CurrentFireMode;
	}
	if (UTOwner == NULL)
	{
		UE_LOG(UT, Warning, TEXT("%s::GetFireStartLoc(): No Owner (died while firing?)"), *GetName());
		return FVector::ZeroVector;
	}
	else
	{
		const bool bIsFirstPerson = Cast<APlayerController>(UTOwner->Controller) != NULL; // FIXMESTEVE TODO: first person view check (need to make sure sync'ed with server)
		FVector BaseLoc;
		if (bFPFireFromCenter && bIsFirstPerson)
		{
			BaseLoc = UTOwner->GetPawnViewLocation();
		}
		else
		{
			BaseLoc = UTOwner->GetActorLocation();
		}

		if (bNetDelayedShot)
		{
			// adjust for delayed shot to position client shot from
			BaseLoc = BaseLoc + UTOwner->GetDelayedShotPosition() - UTOwner->GetActorLocation();
		}
		// ignore offset for instant hit shots in first person
		if (FireOffset.IsZero() || (bIsFirstPerson && bFPIgnoreInstantHitFireOffset && InstantHitInfo.IsValidIndex(FireMode) && InstantHitInfo[FireMode].DamageType != NULL))
		{
			return BaseLoc;
		}
		else
		{
			FVector AdjustedFireOffset;
			switch (GetWeaponHand())
			{
				case HAND_Right:
					AdjustedFireOffset = FireOffset;
					break;
				case HAND_Left:
					AdjustedFireOffset = FireOffset;
					AdjustedFireOffset.Y *= -1.0f;
					break;
				case HAND_Center:
				case HAND_Hidden:
					AdjustedFireOffset = FVector::ZeroVector;
					AdjustedFireOffset.X = FireOffset.X;
					break;
			}
			// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
			FVector FinalLoc = BaseLoc + GetBaseFireRotation().RotateVector(AdjustedFireOffset);
			// trace back towards Instigator's collision, then trace from there to desired location, checking for intervening world geometry
			FCollisionShape Collider;
			if (ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL && ProjClass[CurrentFireMode].GetDefaultObject()->CollisionComp != NULL)
			{
				Collider = FCollisionShape::MakeSphere(ProjClass[CurrentFireMode].GetDefaultObject()->CollisionComp->GetUnscaledSphereRadius());
			}
			else
			{
				Collider = FCollisionShape::MakeSphere(0.0f);
			}
			FCollisionQueryParams Params(FName(TEXT("WeaponStartLoc")), false, UTOwner);
			FHitResult Hit;
			if (GetWorld()->SweepSingleByChannel(Hit, BaseLoc, FinalLoc, FQuat::Identity, COLLISION_TRACE_WEAPON, Collider, Params))
			{
				FinalLoc = Hit.Location - (FinalLoc - BaseLoc).GetSafeNormal();
			}
			return FinalLoc;
		}
	}
}

FRotator AUTWeapon::GetBaseFireRotation()
{
	if (UTOwner == NULL)
	{
		UE_LOG(UT, Warning, TEXT("%s::GetBaseFireRotation(): No Owner (died while firing?)"), *GetName());
		return FRotator::ZeroRotator;
	}
	else if (bNetDelayedShot)
	{
		return UTOwner->GetDelayedShotRotation();
	}
	else
	{
		return UTOwner->GetViewRotation();
	}
}

FRotator AUTWeapon::GetAdjustedAim_Implementation(FVector StartFireLoc)
{
	FRotator BaseAim = GetBaseFireRotation();
	GuessPlayerTarget(StartFireLoc, BaseAim.Vector());
	if (Spread.IsValidIndex(CurrentFireMode) && Spread[CurrentFireMode] > 0.0f)
	{
		// add in any spread
		FRotationMatrix Mat(BaseAim);
		FVector X, Y, Z;
		Mat.GetScaledAxes(X, Y, Z);

		NetSynchRandomSeed();
		float RandY = 0.5f * (FMath::FRand() + FMath::FRand() - 1.f);
		float RandZ = FMath::Sqrt(0.25f - FMath::Square(RandY)) * (FMath::FRand() + FMath::FRand() - 1.f);
		return (X + RandY * Spread[CurrentFireMode] * Y + RandZ * Spread[CurrentFireMode] * Z).Rotation();
	}
	else
	{
		return BaseAim;
	}
}

void AUTWeapon::GuessPlayerTarget(const FVector& StartFireLoc, const FVector& FireDir)
{
	if (Role == ROLE_Authority && UTOwner != NULL)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(UTOwner->Controller);
		if (PC != NULL)
		{
			float MaxRange = 100000.0f; // TODO: calc projectile mode range?
			if (InstantHitInfo.IsValidIndex(CurrentFireMode) && InstantHitInfo[CurrentFireMode].DamageType != NULL)
			{
				MaxRange = InstantHitInfo[CurrentFireMode].TraceRange * 1.2f; // extra since player may miss intended target due to being out of range
			}
			PC->LastShotTargetGuess = UUTGameplayStatics::PickBestAimTarget(PC, StartFireLoc, FireDir, 0.9f, MaxRange);
		}
	}
}

void AUTWeapon::NetSynchRandomSeed()
{
	AUTPlayerController* OwningPlayer = UTOwner ? Cast<AUTPlayerController>(UTOwner->GetController()) : NULL;
	if (OwningPlayer && UTOwner && UTOwner->UTCharacterMovement)
	{
		FMath::RandInit(10000.f*UTOwner->UTCharacterMovement->GetCurrentSynchTime());
	}
}

void AUTWeapon::HitScanTrace(const FVector& StartLocation, const FVector& EndTrace, FHitResult& Hit, float PredictionTime)
{
	ECollisionChannel TraceChannel = COLLISION_TRACE_WEAPONNOCHARACTER;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, StartLocation, EndTrace, TraceChannel, FCollisionQueryParams(GetClass()->GetFName(), true, UTOwner)))
	{
		Hit.Location = EndTrace;
	}
	if (!(Hit.Location - StartLocation).IsNearlyZero())
	{
		AUTCharacter* BestTarget = NULL;
		FVector BestPoint(0.f);
		FVector BestCapsulePoint(0.f);
		float BestCollisionRadius = 0.f;
		for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
		{
			AUTCharacter* Target = Cast<AUTCharacter>(*Iterator);
			if (Target && (Target != UTOwner))
			{
				// find appropriate rewind position, and test against trace from StartLocation to Hit.Location
				FVector TargetLocation = ((PredictionTime > 0.f) && (Role == ROLE_Authority)) ? Target->GetRewindLocation(PredictionTime) : Target->GetActorLocation();

				// now see if trace would hit the capsule
				float CollisionHeight = Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				if (Target->UTCharacterMovement && Target->UTCharacterMovement->bIsFloorSliding)
				{
					TargetLocation.Z = TargetLocation.Z - CollisionHeight + Target->SlideTargetHeight;
					CollisionHeight = Target->SlideTargetHeight;
				}
				float CollisionRadius = Target->GetCapsuleComponent()->GetScaledCapsuleRadius();

				bool bHitTarget = false;
				FVector ClosestPoint(0.f);
				FVector ClosestCapsulePoint = TargetLocation;
				if (CollisionRadius >= CollisionHeight)
				{
					ClosestPoint = FMath::ClosestPointOnSegment(TargetLocation, StartLocation, Hit.Location);
					bHitTarget = ((ClosestPoint - TargetLocation).SizeSquared() < FMath::Square(CollisionHeight));
				}
				else
				{
					FVector CapsuleSegment = FVector(0.f, 0.f, CollisionHeight - CollisionRadius);
					FMath::SegmentDistToSegmentSafe(StartLocation, Hit.Location, TargetLocation - CapsuleSegment, TargetLocation + CapsuleSegment, ClosestPoint, ClosestCapsulePoint);
					bHitTarget = ((ClosestPoint - ClosestCapsulePoint).SizeSquared() < FMath::Square(CollisionRadius));
				}
				if (bHitTarget &&  (!BestTarget || ((ClosestPoint - StartLocation).SizeSquared() < (BestPoint - StartLocation).SizeSquared())))
				{
					BestTarget = Target;
					BestPoint = ClosestPoint;
					BestCapsulePoint = ClosestCapsulePoint;
					BestCollisionRadius = CollisionRadius;
				}
			}
		}
		if (BestTarget)
		{
			// we found a player to hit, so update hit result

			// first find proper hit location on surface of capsule
			float ClosestDistSq = (BestPoint - BestCapsulePoint).SizeSquared();
			float BackDist = FMath::Sqrt(FMath::Max(0.f, BestCollisionRadius*BestCollisionRadius - ClosestDistSq));

			Hit.Location = BestPoint + BackDist * (StartLocation - EndTrace).GetSafeNormal();
			Hit.Normal = (Hit.Location - BestCapsulePoint).GetSafeNormal();
			Hit.ImpactNormal = Hit.Normal; 
			Hit.Actor = BestTarget;
			Hit.bBlockingHit = true;
			Hit.Component = BestTarget->GetCapsuleComponent();
			Hit.ImpactPoint = BestPoint; //FIXME
			Hit.Time = (BestPoint - StartLocation).Size() / (EndTrace - StartLocation).Size();
		}
	}
}

void AUTWeapon::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
	UE_LOG(LogUTWeapon, Verbose, TEXT("%s::FireInstantHit()"), *GetName());

	checkSlow(InstantHitInfo.IsValidIndex(CurrentFireMode));

	const FVector SpawnLocation = GetFireStartLoc();
	const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
	const FVector FireDir = SpawnRotation.Vector();
	const FVector EndTrace = SpawnLocation + FireDir * InstantHitInfo[CurrentFireMode].TraceRange;

	FHitResult Hit;
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(UTOwner->Controller);
	AUTPlayerState* PS = UTOwner->Controller ? Cast<AUTPlayerState>(UTOwner->Controller->PlayerState) : NULL;
	float PredictionTime = UTPC ? UTPC->GetPredictionTime() : 0.f;
	HitScanTrace(SpawnLocation, EndTrace, Hit, PredictionTime);
	if (Role == ROLE_Authority)
	{
		if (PS && (ShotsStatsName != NAME_None))
		{
			PS->ModifyStatsValue(ShotsStatsName, 1);
		}
		UTOwner->SetFlashLocation(Hit.Location, CurrentFireMode);
		// warn bot target, if any
		if (UTPC != NULL)
		{
			APawn* PawnTarget = Cast<APawn>(Hit.Actor.Get());
			if (PawnTarget != NULL)
			{
				UTPC->LastShotTargetGuess = PawnTarget;
			}
			// if not dealing damage, it's the caller's responsibility to send warnings if desired
			if (bDealDamage && UTPC->LastShotTargetGuess != NULL)
			{
				AUTBot* EnemyBot = Cast<AUTBot>(UTPC->LastShotTargetGuess->Controller);
				if (EnemyBot != NULL)
				{
					EnemyBot->ReceiveInstantWarning(UTOwner, FireDir);
				}
			}
		}
		else if (bDealDamage)
		{
			AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
			if (B != NULL)
			{
				APawn* PawnTarget = Cast<APawn>(Hit.Actor.Get());
				if (PawnTarget == NULL)
				{
					PawnTarget = Cast<APawn>(B->GetTarget());
				}
				if (PawnTarget != NULL)
				{
					AUTBot* EnemyBot = Cast<AUTBot>(PawnTarget->Controller);
					if (EnemyBot != NULL)
					{
						EnemyBot->ReceiveInstantWarning(UTOwner, FireDir);
					}
				}
			}
		}
	}
	else if (PredictionTime > 0.f)
	{
		PlayPredictedImpactEffects(Hit.Location);
	}
	if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged && bDealDamage)
	{
		if ((Role == ROLE_Authority) && PS && (HitsStatsName != NAME_None))
		{
			PS->ModifyStatsValue(HitsStatsName, 1);
		}
		Hit.Actor->TakeDamage(InstantHitInfo[CurrentFireMode].Damage, FUTPointDamageEvent(InstantHitInfo[CurrentFireMode].Damage, Hit, FireDir, InstantHitInfo[CurrentFireMode].DamageType, FireDir * GetImpartedMomentumMag(Hit.Actor.Get())), UTOwner->Controller, this);
	}
	if (OutHit != NULL)
	{
		*OutHit = Hit;
	}
}

void AUTWeapon::PlayPredictedImpactEffects(FVector ImpactLoc)
{
	if (!UTOwner)
	{
		return;
	}
	AUTPlayerController* UTPC = Cast<AUTPlayerController>(UTOwner->Controller);
	float SleepTime = UTPC ? UTPC->GetProjectileSleepTime() : 0.f;
	if (SleepTime > 0.f)
	{
		if (GetWorldTimerManager().IsTimerActive(PlayDelayedImpactEffectsHandle))
		{
			// play the delayed effect now, since we are about to replace it
			PlayDelayedImpactEffects();
		}
		FVector SpawnLocation;
		FRotator SpawnRotation;
		GetImpactSpawnPosition(ImpactLoc, SpawnLocation, SpawnRotation);
		DelayedHitScan.ImpactLocation = ImpactLoc;
		DelayedHitScan.FireMode = CurrentFireMode;
		DelayedHitScan.SpawnLocation = SpawnLocation;
		DelayedHitScan.SpawnRotation = SpawnRotation;
		GetWorldTimerManager().SetTimer(PlayDelayedImpactEffectsHandle, this, &AUTWeapon::PlayDelayedImpactEffects, SleepTime, false);
	}
	else
	{
		FVector SpawnLocation;
		FRotator SpawnRotation;
		GetImpactSpawnPosition(ImpactLoc, SpawnLocation, SpawnRotation);
		UTOwner->SetFlashLocation(ImpactLoc, CurrentFireMode);
	}
}

void AUTWeapon::PlayDelayedImpactEffects()
{
	if (UTOwner)
	{
		UTOwner->SetFlashLocation(DelayedHitScan.ImpactLocation, DelayedHitScan.FireMode);
	}
}

float AUTWeapon::GetImpartedMomentumMag(AActor* HitActor)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if ((FriendlyMomentumScaling != 1.f) && GS != NULL && Cast<AUTCharacter>(HitActor) && GS->OnSameTeam(HitActor, UTOwner))
	{
		return  InstantHitInfo[CurrentFireMode].Momentum *FriendlyMomentumScaling;
	}

	return InstantHitInfo[CurrentFireMode].Momentum;
}

void AUTWeapon::K2_FireInstantHit(bool bDealDamage, FHitResult& OutHit)
{
	if (!InstantHitInfo.IsValidIndex(CurrentFireMode))
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s::FireInstantHit(): Fire mode %i doesn't have instant hit info"), *GetName(), int32(CurrentFireMode)), ELogVerbosity::Warning);
	}
	else if (GetUTOwner() != NULL)
	{
		FireInstantHit(bDealDamage, &OutHit);
	}
	else
	{
		FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s::FireInstantHit(): Weapon is not owned (owner died while script was running?)"), *GetName()), ELogVerbosity::Warning);
	}
}

AUTProjectile* AUTWeapon::FireProjectile()
{
	UE_LOG(LogUTWeapon, Verbose, TEXT("%s::FireProjectile()"), *GetName());

	if (GetUTOwner() == NULL)
	{
		UE_LOG(LogUTWeapon, Warning, TEXT("%s::FireProjectile(): Weapon is not owned (owner died during firing sequence)"), *GetName());
		return NULL;
	}

	checkSlow(ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL);
	if (Role == ROLE_Authority)
	{
		UTOwner->IncrementFlashCount(CurrentFireMode);
		AUTPlayerState* PS = UTOwner->Controller ? Cast<AUTPlayerState>(UTOwner->Controller->PlayerState) : NULL;
		if (PS && (ShotsStatsName != NAME_None))
		{
			PS->ModifyStatsValue(ShotsStatsName, 1);
		}
	}
	// spawn the projectile at the muzzle
	const FVector SpawnLocation = GetFireStartLoc();
	const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);
	return SpawnNetPredictedProjectile(ProjClass[CurrentFireMode], SpawnLocation, SpawnRotation);
}

AUTProjectile* AUTWeapon::SpawnNetPredictedProjectile(TSubclassOf<AUTProjectile> ProjectileClass, FVector SpawnLocation, FRotator SpawnRotation)
{
	//DrawDebugSphere(GetWorld(), SpawnLocation, 10, 10, FColor::Green, true);
	AUTPlayerController* OwningPlayer = UTOwner ? Cast<AUTPlayerController>(UTOwner->GetController()) : NULL;
	float CatchupTickDelta = 
		((GetNetMode() != NM_Standalone) && OwningPlayer)
		? OwningPlayer->GetPredictionTime()
		: 0.f;
/*
	if (OwningPlayer)
	{
		float CurrentMoveTime = (UTOwner && UTOwner->UTCharacterMovement) ? UTOwner->UTCharacterMovement->GetCurrentSynchTime() : GetWorld()->GetTimeSeconds();
		if (UTOwner->Role < ROLE_Authority)
		{
			UE_LOG(UT, Warning, TEXT("CLIENT SpawnNetPredictedProjectile at %f yaw %f "), CurrentMoveTime, SpawnRotation.Yaw);
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("SERVER SpawnNetPredictedProjectile at %f yaw %f TIME %f"), CurrentMoveTime, SpawnRotation.Yaw, GetWorld()->GetTimeSeconds());
		}
	}
*/
	if ((CatchupTickDelta > 0.f) && (Role != ROLE_Authority))
	{
		float SleepTime = OwningPlayer->GetProjectileSleepTime();
		if (SleepTime > 0.f)
		{
			// lag is so high need to delay spawn
			if (!GetWorldTimerManager().IsTimerActive(SpawnDelayedFakeProjHandle))
			{
				DelayedProjectile.ProjectileClass = ProjectileClass;
				DelayedProjectile.SpawnLocation = SpawnLocation;
				DelayedProjectile.SpawnRotation = SpawnRotation;
				GetWorldTimerManager().SetTimer(SpawnDelayedFakeProjHandle, this, &AUTWeapon::SpawnDelayedFakeProjectile, SleepTime, false);
			}
			return NULL;
		}
	}
	FActorSpawnParameters Params;
	Params.Instigator = UTOwner;
	Params.Owner = UTOwner;
	Params.bNoCollisionFail = true; // we already checked this in GetFireStartLoc()
	AUTProjectile* NewProjectile = 
		((Role == ROLE_Authority) || (CatchupTickDelta > 0.f))
		? GetWorld()->SpawnActor<AUTProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, Params)
		: NULL;
	if (NewProjectile)
	{
		if (Role == ROLE_Authority)
		{
			NewProjectile->HitsStatsName = HitsStatsName;
			if ((CatchupTickDelta > 0.f) && NewProjectile->ProjectileMovement)
			{
				// server ticks projectile to match with when client actually fired
				NewProjectile->ProjectileMovement->TickComponent(CatchupTickDelta, LEVELTICK_All, NULL);
				NewProjectile->SetForwardTicked(true);
				if (NewProjectile->GetLifeSpan() > 0.f)
				{
					NewProjectile->SetLifeSpan(FMath::Max(0.001f, NewProjectile->GetLifeSpan() - CatchupTickDelta));
				}
			}
			else
			{
				NewProjectile->SetForwardTicked(false);
			}
		}
		else
		{
			NewProjectile->InitFakeProjectile(OwningPlayer);
			NewProjectile->SetLifeSpan(FMath::Min(NewProjectile->GetLifeSpan(), 2.f * FMath::Max(0.f, CatchupTickDelta)));
		}
	}
	return NewProjectile;
}

void AUTWeapon::SpawnDelayedFakeProjectile()
{
	AUTPlayerController* OwningPlayer = UTOwner ? Cast<AUTPlayerController>(UTOwner->GetController()) : NULL;
	if (OwningPlayer)
	{
		FActorSpawnParameters Params;
		Params.Instigator = UTOwner;
		Params.Owner = UTOwner;
		Params.bNoCollisionFail = true; // we already checked this in GetFireStartLoc()
		AUTProjectile* NewProjectile = GetWorld()->SpawnActor<AUTProjectile>(DelayedProjectile.ProjectileClass, DelayedProjectile.SpawnLocation, DelayedProjectile.SpawnRotation, Params);
		if (NewProjectile)
		{
			NewProjectile->InitFakeProjectile(OwningPlayer);
			NewProjectile->SetLifeSpan(FMath::Min(NewProjectile->GetLifeSpan(), 0.002f * FMath::Max(0.f, OwningPlayer->MaxPredictionPing + OwningPlayer->PredictionFudgeFactor)));
		}
	}
}

float AUTWeapon::GetRefireTime(uint8 FireModeNum)
{
	if (FireInterval.IsValidIndex(FireModeNum))
	{
		float Result = FireInterval[FireModeNum];
		if (UTOwner != NULL)
		{
			Result /= UTOwner->GetFireRateMultiplier();
		}
		return FMath::Max<float>(0.01f, Result);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Invalid firing mode %i in %s::GetRefireTime()"), int32(FireModeNum), *GetName());
		return 0.1f;
	}
}

void AUTWeapon::UpdateTiming()
{
	CurrentState->UpdateTiming();
}

bool AUTWeapon::StackPickup_Implementation(AUTInventory* ContainedInv)
{
	AddAmmo(ContainedInv != NULL ? Cast<AUTWeapon>(ContainedInv)->Ammo : GetClass()->GetDefaultObject<AUTWeapon>()->Ammo);
	return true;
}

bool AUTWeapon::ShouldLagRot()
{
	return bProceduralLagRotation;
}

float AUTWeapon::LagWeaponRotation(float NewValue, float LastValue, float DeltaTime, float MaxDiff, int32 Index)
{
	// check if NewValue is clockwise from LastValue
	NewValue = FMath::UnwindDegrees(NewValue);
	LastValue = FMath::UnwindDegrees(LastValue);

	float LeadMag = 0.f;
	float RotDiff = NewValue - LastValue;
	if ((RotDiff == 0.f) || (OldRotDiff[Index] == 0.f))
	{
		LeadMag = ShouldLagRot() ? OldLeadMag[Index] : 0.f;
		if ((RotDiff == 0.f) && (OldRotDiff[Index] == 0.f))
		{
			OldMaxDiff[Index] = 0.f;
		}
	}
	else if ((RotDiff > 0.f) == (OldRotDiff[Index] > 0.f))
	{
		if (ShouldLagRot())
		{
			MaxDiff = FMath::Min(1.f, FMath::Abs(RotDiff) / (66.f * DeltaTime)) * MaxDiff;
			if (OldMaxDiff[Index] != 0.f)
			{
				MaxDiff = FMath::Max(OldMaxDiff[Index], MaxDiff);
			}

			OldMaxDiff[Index] = MaxDiff;
			LeadMag = (NewValue > LastValue) ? -1.f * MaxDiff : MaxDiff;
		}
		LeadMag = (DeltaTime < 1.f / RotChgSpeed)
					? LeadMag = (1.f- RotChgSpeed*DeltaTime)*OldLeadMag[Index] + RotChgSpeed*DeltaTime*LeadMag
					: LeadMag = 0.f;
	}
	else
	{
		OldMaxDiff[Index] = 0.f;
		if (DeltaTime < 1.f/ReturnChgSpeed)
		{
			LeadMag = (1.f - ReturnChgSpeed*DeltaTime)*OldLeadMag[Index] + ReturnChgSpeed*DeltaTime*LeadMag;
		}
	}
	OldLeadMag[Index] = LeadMag;
	OldRotDiff[Index] = RotDiff;

	return LeadMag;
}

void AUTWeapon::Tick(float DeltaTime)
{
	// try to gracefully detect being active with no owner, which should never happen because we should always end up going through Removed() and going to the inactive state
	if (CurrentState != InactiveState && (UTOwner == NULL || UTOwner->bPendingKillPending) && CurrentState != NULL)
	{
		UE_LOG(UT, Warning, TEXT("%s lost Owner while active (state %s"), *GetName(), *GetNameSafe(CurrentState));
		GotoState(InactiveState);
	}

	Super::Tick(DeltaTime);

	// note that this relies on us making BeginPlay() always called before first tick; see UUTGameEngine::LoadMap()
	if (CurrentState != InactiveState)
	{
		CurrentState->Tick(DeltaTime);
	}

	TickZoom(DeltaTime);
}

void AUTWeapon::UpdateViewBob(float DeltaTime)
{
	AUTPlayerController* MyPC = UTOwner ? UTOwner->GetLocalViewer() : NULL;
	if (MyPC && Mesh && (UTOwner->GetWeapon() == this) && ShouldPlay1PVisuals())
	{
		// if weapon is up in first person, view bob with movement
		if (GetWeaponHand() != HAND_Hidden)
		{
			USkeletalMeshComponent* BobbedMesh = (HandsAttachSocket != NAME_None) ? UTOwner->FirstPersonMesh : Mesh;
			if (FirstPMeshOffset.IsZero())
			{
				FirstPMeshOffset = BobbedMesh->GetRelativeTransform().GetLocation();
				FirstPMeshRotation = BobbedMesh->GetRelativeTransform().Rotator();
			}
			FVector ScaledMeshOffset = FirstPMeshOffset;
			const float FOVScaling = (MyPC != NULL) ? ((MyPC->PlayerCameraManager->GetFOVAngle() - 100.f) * 0.05f) : 1.0f;
			if (FOVScaling > 0.f)
			{
				ScaledMeshOffset.X *= (1.f + (FOVOffset.X - 1.f) * FOVScaling);
				ScaledMeshOffset.Y *= (1.f + (FOVOffset.Y - 1.f) * FOVScaling);
				ScaledMeshOffset.Z *= (1.f + (FOVOffset.Z - 1.f) * FOVScaling);
			}

			BobbedMesh->SetRelativeLocation(ScaledMeshOffset);
			FVector BobOffset = UTOwner->GetWeaponBobOffset(DeltaTime, this);
			BobbedMesh->SetWorldLocation(BobbedMesh->GetComponentLocation() + BobOffset);

			FRotator NewRotation = UTOwner ? UTOwner->GetControlRotation() : FRotator(0.f, 0.f, 0.f);
			FRotator FinalRotation = NewRotation;

			// Add some rotation leading
			if (UTOwner && UTOwner->Controller)
			{
				FinalRotation.Yaw = LagWeaponRotation(NewRotation.Yaw, LastRotation.Yaw, DeltaTime, MaxYawLag, 0);
				FinalRotation.Pitch = LagWeaponRotation(NewRotation.Pitch, LastRotation.Pitch, DeltaTime, MaxPitchLag, 1);
				FinalRotation.Roll = NewRotation.Roll;
			}
			LastRotation = NewRotation;
			BobbedMesh->SetRelativeRotation(FinalRotation + FirstPMeshRotation);
		}
		else
		{
			// for first person footsteps
			UTOwner->GetWeaponBobOffset(DeltaTime, this);
		}
	}
}

void AUTWeapon::Destroyed()
{
	Super::Destroyed();

	// this makes sure timers, etc go away
	GotoState(InactiveState);
}

bool AUTWeapon::CanFireAgain()
{
	return (GetUTOwner() && (GetUTOwner()->GetPendingWeapon() == NULL) && HasAmmo(GetCurrentFireMode()));
}

bool AUTWeapon::HandleContinuedFiring()
{
	if (!CanFireAgain() || !GetUTOwner()->IsPendingFire(GetCurrentFireMode()))
	{
		GotoActiveState();
		return false;
	}

	OnContinuedFiring();
	return true;
}

// hooks meant for subclasses/blueprints, empty by default
void AUTWeapon::OnStartedFiring_Implementation()
{}
void AUTWeapon::OnStoppedFiring_Implementation()
{}
void AUTWeapon::OnContinuedFiring_Implementation()
{}
void AUTWeapon::OnMultiPress_Implementation(uint8 OtherFireMode)
{}

void AUTWeapon::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTWeapon, Ammo, COND_None);
	DOREPLIFETIME_CONDITION(AUTWeapon, MaxAmmo, COND_OwnerOnly);
	DOREPLIFETIME(AUTWeapon, AttachmentType);

	DOREPLIFETIME_CONDITION(AUTWeapon, ZoomCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTWeapon, ZoomState, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTWeapon, CurrentZoomMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTWeapon, ZoomTime, COND_InitialOnly);
}

FLinearColor AUTWeapon::GetCrosshairColor(UUTHUDWidget* WeaponHudWidget) const
{
	FLinearColor CrosshairColor = FLinearColor::White;
	return WeaponHudWidget->UTHUDOwner->GetCrosshairColor(CrosshairColor);
}

bool AUTWeapon::ShouldDrawFFIndicator(APlayerController* Viewer, AUTPlayerState *& HitPlayerState) const
{
	bool bDrawFriendlyIndicator = false;
	HitPlayerState = nullptr;
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		FVector CameraLoc;
		FRotator CameraRot;
		Viewer->GetPlayerViewPoint(CameraLoc, CameraRot);
		FHitResult Hit;
		GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, CameraLoc + CameraRot.Vector() * 50000.0f, COLLISION_TRACE_WEAPON, FCollisionQueryParams(FName(TEXT("CrosshairFriendIndicator")), true, UTOwner));
		if (Hit.Actor != NULL)
		{
			AUTCharacter* Char = Cast<AUTCharacter>(Hit.Actor.Get());
			if (Char != NULL)
			{
				bDrawFriendlyIndicator = GS->OnSameTeam(Hit.Actor.Get(), UTOwner);

				if (Char != NULL && !Char->IsFeigningDeath())
				{
					if (Char->PlayerState != nullptr)
					{
						HitPlayerState = Cast<AUTPlayerState>(Char->PlayerState);
					}
					else if (Char->DrivenVehicle != nullptr && Char->DrivenVehicle->PlayerState != nullptr)
					{
						HitPlayerState = Cast<AUTPlayerState>(Char->DrivenVehicle->PlayerState);
					}
				}
			}
		}
	}
	return bDrawFriendlyIndicator;
}

float AUTWeapon::GetCrosshairScale(AUTHUD* HUD)
{
	return HUD->GetCrosshairScale();
}

void AUTWeapon::DrawWeaponCrosshair_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta)
{
	bool bDrawCrosshair = true;
	for (int32 i = 0; i < FiringState.Num(); i++)
	{
		bDrawCrosshair = FiringState[i]->DrawHUD(WeaponHudWidget) && bDrawCrosshair;
	}

	if (bDrawCrosshair && WeaponHudWidget && WeaponHudWidget->UTHUDOwner)
	{
		UTexture2D* CrosshairTexture = WeaponHudWidget->UTHUDOwner->DefaultCrosshairTex;
		if (CrosshairTexture != NULL)
		{
			float W = CrosshairTexture->GetSurfaceWidth();
			float H = CrosshairTexture->GetSurfaceHeight();
			float CrosshairScale = GetCrosshairScale(WeaponHudWidget->UTHUDOwner);

			// draw a different indicator if there is a friendly where the camera is pointing
			AUTPlayerState* PS;
			if (ShouldDrawFFIndicator(WeaponHudWidget->UTHUDOwner->PlayerOwner, PS))
			{
				WeaponHudWidget->DrawTexture(WeaponHudWidget->UTHUDOwner->HUDAtlas, 0, 0, W * CrosshairScale, H * CrosshairScale, 407, 940, 72, 72, 1.0, FLinearColor::Green, FVector2D(0.5f, 0.5f));
			}
			else
			{
				UUTCrosshair* Crosshair = WeaponHudWidget->UTHUDOwner->GetCrosshair(this);
				FCrosshairInfo* CrosshairInfo = WeaponHudWidget->UTHUDOwner->GetCrosshairInfo(this);

				if (Crosshair != nullptr && CrosshairInfo != nullptr)
				{
					Crosshair->DrawCrosshair(WeaponHudWidget->GetCanvas(), this, RenderDelta, GetCrosshairScale(WeaponHudWidget->UTHUDOwner) * CrosshairInfo->Scale, WeaponHudWidget->UTHUDOwner->GetCrosshairColor(CrosshairInfo->Color));
				}
				else
				{
					WeaponHudWidget->DrawTexture(CrosshairTexture, 0, 0, W * CrosshairScale, H * CrosshairScale, 0.0, 0.0, 16, 16, 1.0, GetCrosshairColor(WeaponHudWidget), FVector2D(0.5f, 0.5f));
				}
				UpdateCrosshairTarget(PS, WeaponHudWidget, RenderDelta);
			}
		}
	}
}

void AUTWeapon::UpdateCrosshairTarget(AUTPlayerState* NewCrosshairTarget, UUTHUDWidget* WeaponHudWidget, float RenderDelta)
{
	if (NewCrosshairTarget != NULL)
	{
		TargetPlayerState = NewCrosshairTarget;
		TargetLastSeenTime = GetWorld()->GetTimeSeconds();
	}

	if (TargetPlayerState != NULL)
	{
		float TimeSinceSeen = GetWorld()->GetTimeSeconds() - TargetLastSeenTime;
		static float MAXNAMEDRAWTIME = 0.3f;
		if (TimeSinceSeen < MAXNAMEDRAWTIME)
		{
			static float MAXNAMEFULLALPHA = 0.22f;
			float Alpha = (TimeSinceSeen < MAXNAMEFULLALPHA) ? 1.f : (1.f - ((TimeSinceSeen - MAXNAMEFULLALPHA) / (MAXNAMEDRAWTIME - MAXNAMEFULLALPHA)));

			float H = WeaponHudWidget->UTHUDOwner->DefaultCrosshairTex->GetSurfaceHeight();
			UFont* Font = WeaponHudWidget->UTHUDOwner->MediumFont;
			FText PlayerName = FText::FromString(TargetPlayerState->PlayerName);
			WeaponHudWidget->DrawText(PlayerName, 0, H * 2, Font, false, FVector2D(0,0), FLinearColor::Black, true, FLinearColor::Black,1.0, Alpha, FLinearColor::Red, ETextHorzPos::Center);
		}
		else
		{
			TargetPlayerState = NULL;
		}
	}
}

TArray<UMeshComponent*> AUTWeapon::Get1PMeshes_Implementation() const
{
	TArray<UMeshComponent*> Result;
	Result.Add(Mesh);
	Result.Add(OverlayMesh);
	return Result;
}

void AUTWeapon::UpdateOverlaysShared(AActor* WeaponActor, AUTCharacter* InOwner, USkeletalMeshComponent* InMesh, USkeletalMeshComponent*& InOverlayMesh) const
{
	AUTGameState* GS = WeaponActor ? WeaponActor->GetWorld()->GetGameState<AUTGameState>() : NULL;
	if (GS != NULL && InOwner != NULL && InMesh != NULL)
	{
		UMaterialInterface* TopOverlay = GS->GetFirstOverlay(InOwner->GetWeaponOverlayFlags(), Cast<AUTWeapon>(WeaponActor) != NULL);
		if (TopOverlay != NULL)
		{
			if (InOverlayMesh == NULL)
			{
				InOverlayMesh = DuplicateObject<USkeletalMeshComponent>(InMesh, WeaponActor);
				InOverlayMesh->AttachParent = NULL; // this gets copied but we don't want it to be
				{
					// TODO: scary that these get copied, need an engine solution and/or safe way to duplicate objects during gameplay
					InOverlayMesh->PrimaryComponentTick = InOverlayMesh->GetClass()->GetDefaultObject<USkeletalMeshComponent>()->PrimaryComponentTick;
					InOverlayMesh->PostPhysicsComponentTick = InOverlayMesh->GetClass()->GetDefaultObject<USkeletalMeshComponent>()->PostPhysicsComponentTick;
				}
				InOverlayMesh->SetMasterPoseComponent(InMesh);
			}
			if (!InOverlayMesh->IsRegistered())
			{
				InOverlayMesh->RegisterComponent();
				InOverlayMesh->AttachTo(InMesh, NAME_None, EAttachLocation::SnapToTarget);
				InOverlayMesh->SetWorldScale3D(InMesh->GetComponentScale());
			}
			for (int32 i = 0; i < InOverlayMesh->GetNumMaterials(); i++)
			{
				InOverlayMesh->SetMaterial(i, TopOverlay);
			}
		}
		else if (InOverlayMesh != NULL && InOverlayMesh->IsRegistered())
		{
			InOverlayMesh->DetachFromParent();
			InOverlayMesh->UnregisterComponent();
		}
	}
}
void AUTWeapon::UpdateOverlays()
{
	UpdateOverlaysShared(this, GetUTOwner(), Mesh, OverlayMesh);
}

void AUTWeapon::SetSkin(UMaterialInterface* NewSkin)
{
	// FIXME: workaround for poor GetNumMaterials() implementation
	int32 NumMats = Mesh->GetNumMaterials();
	if (Mesh->SkeletalMesh != NULL)
	{
		NumMats = FMath::Max<int32>(NumMats, Mesh->SkeletalMesh->Materials.Num());
	}
	// save off existing materials if we haven't yet done so
	while (SavedMeshMaterials.Num() < NumMats)
	{
		SavedMeshMaterials.Add(Mesh->GetMaterial(SavedMeshMaterials.Num()));
	}
	if (NewSkin != NULL)
	{
		for (int32 i = 0; i < NumMats; i++)
		{
			Mesh->SetMaterial(i, NewSkin);
		}
	}
	else
	{
		for (int32 i = 0; i < NumMats; i++)
		{
			Mesh->SetMaterial(i, SavedMeshMaterials[i]);
		}
	}
}

float AUTWeapon::GetDamageRadius_Implementation(uint8 TestMode) const
{
	if (ProjClass.IsValidIndex(TestMode) && ProjClass[TestMode] != NULL)
	{
		return ProjClass[TestMode].GetDefaultObject()->DamageParams.OuterRadius;
	}
	else
	{
		return 0.0f;
	}
}

float AUTWeapon::BotDesireability_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P == NULL)
	{
		return 0.0f;
	}
	else
	{
		float Desire = BasePickupDesireability;

		AUTBot* B = Cast<AUTBot>(P->Controller);
		if (B != NULL && B->IsFavoriteWeapon(GetClass()))
		{
			Desire *= 1.5f;
		}

		// see if bot already has a weapon of this type
		AUTWeapon* AlreadyHas = P->FindInventoryType<AUTWeapon>(GetClass());
		if (AlreadyHas != NULL)
		{
			//if (Bot.bHuntPlayer)
			//	return 0;
			if (Ammo == 0 || AlreadyHas->MaxAmmo == 0)
			{
				// weapon pickup doesn't give ammo and/or weapon has infinite ammo so we don't care once we have it
				return 0;
			}
			else if (AlreadyHas->Ammo >= AlreadyHas->MaxAmmo)
			{
				return 0.25f * Desire;
			}
			// bot wants this weapon for the ammo it holds
			else
			{
				return Desire * FMath::Max<float>(0.25f, float(AlreadyHas->MaxAmmo - AlreadyHas->Ammo) / float(AlreadyHas->MaxAmmo));
			}
		}
		//else if (Bot.bHuntPlayer && (desire * 0.833 < P.Weapon.AIRating - 0.1))
		//{
		//	return 0;
		//}
		else
		{
			return (B != NULL && B->NeedsWeapon()) ? (2.0f * Desire) : Desire;
		}
	}
}
float AUTWeapon::DetourWeight_Implementation(APawn* Asker, AActor* Pickup, float PathDistance) const
{
	AUTCharacter* P = Cast<AUTCharacter>(Asker);
	if (P == NULL)
	{
		return 0.0f;
	}
	// detour if currently equipped weapon
	else if (P->GetWeaponClass() == GetClass())
	{
		return BotDesireability(Asker, Pickup, PathDistance);
	}
	else
	{
		// detour if favorite weapon
		AUTBot* B = Cast<AUTBot>(P->Controller);
		if (B != NULL && B->IsFavoriteWeapon(GetClass()))
		{
			return BotDesireability(Asker, Pickup, PathDistance);
		}
		else
		{
			// detour if out of ammo for this weapon
			AUTWeapon* AlreadyHas = P->FindInventoryType<AUTWeapon>(GetClass());
			if (AlreadyHas == NULL || AlreadyHas->Ammo == 0)
			{
				return BotDesireability(Asker, Pickup, PathDistance);
			}
			else
			{
				// otherwise not important enough
				return 0.0f;
			}
		}
	}
}

float AUTWeapon::GetAISelectRating_Implementation()
{
	return HasAnyAmmo() ? BaseAISelectRating : 0.0f;
}

float AUTWeapon::SuggestAttackStyle_Implementation()
{
	return 0.0f;
}
float AUTWeapon::SuggestDefenseStyle_Implementation()
{
	return 0.0f;
}

bool AUTWeapon::IsPreparingAttack_Implementation()
{
	return false;
}

bool AUTWeapon::ShouldAIDelayFiring_Implementation()
{
	return false;
}

bool AUTWeapon::CanAttack_Implementation(AActor* Target, const FVector& TargetLoc, bool bDirectOnly, bool bPreferCurrentMode, uint8& BestFireMode, FVector& OptimalTargetLoc)
{
	OptimalTargetLoc = TargetLoc;
	bool bVisible = false;
	AUTBot* B = Cast<AUTBot>(UTOwner->Controller);
	if (B != NULL)
	{
		APawn* TargetPawn = Cast<APawn>(Target);
		if (TargetPawn != NULL && TargetLoc == Target->GetActorLocation() && B->IsEnemyVisible(TargetPawn))
		{
			bVisible = true;
		}
		else
		{
			// by default bots do not try shooting enemies when the enemy info is stale
			// since even if the target location is visible the enemy is probably not near there anymore
			// subclasses can override if their fire mode(s) are suitable for speculative or predictive firing
			const FBotEnemyInfo* EnemyInfo = (TargetPawn != NULL) ? B->GetEnemyInfo(TargetPawn, true) : NULL;
			if (EnemyInfo != NULL && GetWorld()->TimeSeconds - EnemyInfo->LastFullUpdateTime > 1.0f)
			{
				bVisible = false;
			}
			else
			{
				bVisible = B->UTLineOfSightTo(Target, FVector::ZeroVector, false, TargetLoc);
			}
		}
	}
	else
	{
		const FVector StartLoc = GetFireStartLoc();
		FCollisionQueryParams Params(FName(TEXT("CanAttack")), false, Instigator);
		Params.AddIgnoredActor(Target);
		bVisible = !GetWorld()->LineTraceTestByChannel(StartLoc, TargetLoc, COLLISION_TRACE_WEAPON, Params);
	}
	if (bVisible)
	{
		// skip zoom modes by default
		TArray< uint8, TInlineAllocator<4> > ValidAIModes;
		for (uint8 i = 0; i < GetNumFireModes(); i++)
		{
			if (Cast<UUTWeaponStateZooming>(FiringState[i]) == NULL)
			{
				ValidAIModes.Add(i);
			}
		}
		if (!bPreferCurrentMode && ValidAIModes.Num() > 0)
		{
			BestFireMode = ValidAIModes[FMath::RandHelper(ValidAIModes.Num())];
		}
		return true;
	}
	else
	{
		return false;
	}
}

void AUTWeapon::NotifyKillWhileHolding_Implementation(TSubclassOf<UDamageType> DmgType)
{
}

int32 AUTWeapon::GetWeaponKillStats(AUTPlayerState* PS) const
{
	int32 KillCount = 0;
	if (PS)
	{
		if (KillStatsName != NAME_None)
		{
			KillCount += PS->GetStatsValue(KillStatsName);
		}
		if (AltKillStatsName != NAME_None)
		{
			KillCount += PS->GetStatsValue(AltKillStatsName);
		}
	}
	return KillCount;
}

int32 AUTWeapon::GetWeaponDeathStats(AUTPlayerState* PS) const
{
	int32 DeathCount = 0;
	if (PS)
	{
		if (DeathStatsName != NAME_None)
		{
			DeathCount += PS->GetStatsValue(DeathStatsName);
		}
		if (AltDeathStatsName != NAME_None)
		{
			DeathCount += PS->GetStatsValue(AltDeathStatsName);
		}
	}
	return DeathCount;
}

float AUTWeapon::GetWeaponHitsStats(AUTPlayerState* PS) const
{
	return (PS && (HitsStatsName != NAME_None)) ? PS->GetStatsValue(HitsStatsName) : 0.f;
}

float AUTWeapon::GetWeaponShotsStats(AUTPlayerState* PS) const
{
	return (PS && (ShotsStatsName != NAME_None)) ? PS->GetStatsValue(ShotsStatsName) : 0.f;
}

// TEMP for testing 1p offsets
void AUTWeapon::TestWeaponLoc(float X, float Y, float Z)
{
	Mesh->SetRelativeLocation(FVector(X, Y, Z));
}
void AUTWeapon::TestWeaponRot(float Pitch, float Yaw, float Roll)
{
	Mesh->SetRelativeRotation(FRotator(Pitch, Yaw, Roll));
}
void AUTWeapon::TestWeaponScale(float X, float Y, float Z)
{
	Mesh->SetRelativeScale3D(FVector(X, Y, Z));
}

void AUTWeapon::FiringInfoUpdated_Implementation(uint8 InFireMode, uint8 FlashCount, FVector InFlashLocation)
{
	if (FlashCount > 0 || !InFlashLocation.IsZero())
	{
		CurrentFireMode = InFireMode;
		PlayFiringEffects();
	}
	else
	{
		StopFiringEffects();
	}
}

void AUTWeapon::FiringExtraUpdated_Implementation(uint8 NewFlashExtra, uint8 InFireMode)
{

}

void AUTWeapon::FiringEffectsUpdated_Implementation(uint8 InFireMode, FVector InFlashLocation)
{
	FVector SpawnLocation;
	FRotator SpawnRotation;
	GetImpactSpawnPosition(InFlashLocation, SpawnLocation, SpawnRotation);
	PlayImpactEffects(InFlashLocation, InFireMode, SpawnLocation, SpawnRotation);
}

void AUTWeapon::TickZoom(float DeltaTime)
{
	if (GetUTOwner() != nullptr && ZoomModes.IsValidIndex(CurrentZoomMode))
	{
		if (ZoomState != EZoomState::EZS_NotZoomed)
		{
			if (ZoomState == EZoomState::EZS_ZoomingIn)
			{
				ZoomTime += DeltaTime;

				if (ZoomTime >= ZoomModes[CurrentZoomMode].Time)
				{
					OnZoomedIn();
				}
			}
			else if (ZoomState == EZoomState::EZS_ZoomingOut)
			{
				ZoomTime -= DeltaTime;

				if (ZoomTime <= 0.0f)
				{
					OnZoomedOut();
				}
			}
			ZoomTime = FMath::Clamp(ZoomTime, 0.0f, ZoomModes[CurrentZoomMode].Time);

			//Do the FOV change
			if (GetNetMode() != NM_DedicatedServer)
			{
				float StartFov = ZoomModes[CurrentZoomMode].StartFOV;
				float EndFov = ZoomModes[CurrentZoomMode].EndFOV;

				//Use the players default FOV if the FOV is zero
				AUTPlayerController* UTPC = Cast<AUTPlayerController>(GetWorld()->GetFirstPlayerController());
				if (UTPC != nullptr)
				{
					if (StartFov == 0.0f)
					{
						StartFov = UTPC->ConfigDefaultFOV;
					}
					if (EndFov == 0.0f)
					{
						EndFov = UTPC->ConfigDefaultFOV;
					}
				}

				//Calculate the FOV based on the zoom time
				float FOV = 90.0f;
				if (ZoomModes[CurrentZoomMode].Time == 0.0f)
				{
					FOV = StartFov;
				}
				else
				{
					FOV = FMath::Lerp(StartFov, EndFov, ZoomTime / ZoomModes[CurrentZoomMode].Time);
					FOV = FMath::Clamp(FOV, EndFov, StartFov);
				}

				//Set the FOV
				AUTPlayerCameraManager* Camera = Cast<AUTPlayerCameraManager>(GetUTOwner()->GetPlayerCameraManager());
				if (Camera != NULL && Camera->GetCameraStyleWithOverrides() == FName(TEXT("Default")))
				{
					Camera->SetFOV(FOV);
				}
			}
		}
	}
}


void AUTWeapon::LocalSetZoomMode(uint8 NewZoomMode)
{
	if (ZoomModes.IsValidIndex(CurrentZoomMode))
	{
		CurrentZoomMode = NewZoomMode;
	}
	else
	{
		UE_LOG(LogUTWeapon, Warning, TEXT("%s::LocalSetZoomMode(): Invalid Zoom Mode: %d"), *GetName(), NewZoomMode);
	}
}

void AUTWeapon::SetZoomMode(uint8 NewZoomMode)
{
	//Only Locally controlled players set the zoom mode so the server stays in sync
	if (GetUTOwner() && GetUTOwner()->IsLocallyControlled() && CurrentZoomMode != NewZoomMode)
	{
		if (Role < ROLE_Authority)
		{
			ServerSetZoomMode(NewZoomMode);
		}
		LocalSetZoomMode(NewZoomMode);
	}
}

bool AUTWeapon::ServerSetZoomMode_Validate(uint8 NewZoomMode) { return true; }
void AUTWeapon::ServerSetZoomMode_Implementation(uint8 NewZoomMode)
{
	LocalSetZoomMode(NewZoomMode);
}

void AUTWeapon::LocalSetZoomState(uint8 NewZoomState)
{
	if (ZoomModes.IsValidIndex(CurrentZoomMode))
	{
		if (NewZoomState != ZoomState)
		{
			ZoomState = (EZoomState::Type)NewZoomState;

			//Need to reset the zoom time since this state might be skipped on spec clients if states switch too fast
			if (ZoomState == EZoomState::EZS_NotZoomed)
			{
				ZoomCount++;
				OnRep_ZoomCount();
			}
			OnRep_ZoomState();
		}
	}
	else
	{
		UE_LOG(LogUTWeapon, Warning, TEXT("%s::LocalSetZoomState(): Invalid Zoom Mode: %d"), *GetName(), CurrentZoomMode);
	}
}

void AUTWeapon::SetZoomState(TEnumAsByte<EZoomState::Type> NewZoomState)
{
	//Only Locally controlled players set the zoom state so the server stays in sync
	if (GetUTOwner() && GetUTOwner()->IsLocallyControlled() && NewZoomState != ZoomState)
	{
		if (Role < ROLE_Authority)
		{
			ServerSetZoomState(NewZoomState);
		}
		LocalSetZoomState(NewZoomState);
	}
}

bool AUTWeapon::ServerSetZoomState_Validate(uint8 NewZoomState) { return true; }
void AUTWeapon::ServerSetZoomState_Implementation(uint8 NewZoomState)
{
	LocalSetZoomState(NewZoomState);
}

void AUTWeapon::OnRep_ZoomState_Implementation()
{
	if (GetNetMode() != NM_DedicatedServer && ZoomState == EZoomState::EZS_NotZoomed && GetUTOwner() && GetUTOwner()->GetPlayerCameraManager())
	{
		GetUTOwner()->GetPlayerCameraManager()->UnlockFOV();
	}
}

void AUTWeapon::OnRep_ZoomCount()
{
	//For spectators we don't want to clear the time if ZoomTime was just replicated (COND_InitialOnly). Can't do custom rep or we'll loose the COND_SkipOwner
	//CreationTime will be 0 for regular spectator, for demo rec it will be close to GetWorld()->TimeSeconds
	if (CreationTime != 0.0f && GetWorld()->TimeSeconds - CreationTime > 0.2)
	{
		ZoomTime = 0.0f;
	}
}

void AUTWeapon::OnZoomedIn_Implementation()
{
	SetZoomState(EZoomState::EZS_Zoomed);
}

void AUTWeapon::OnZoomedOut_Implementation()
{
	SetZoomState(EZoomState::EZS_NotZoomed);
}