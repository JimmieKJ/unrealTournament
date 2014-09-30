// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTWeaponState.h"
#include "UTWeaponStateFiring.h"
#include "UTWeaponStateActive.h"
#include "UTWeaponStateEquipping.h"
#include "UTWeaponStateUnequipping.h"
#include "UTWeaponStateInactive.h"
#include "UTWeaponAttachment.h"
#include "UnrealNetwork.h"
#include "UTHUDWidget.h"
#include "EditorSupportDelegates.h"
#include "Particles/ParticleSystemComponent.h"
#include "UTImpactEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogUTWeapon, Log, All);

AUTWeapon::AUTWeapon(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP.DoNotCreateDefaultSubobject(TEXT("PickupMesh0")))
{
	AmmoCost.Add(1);
	AmmoCost.Add(1);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;

	bWeaponStay = true;

	Ammo = 20;
	MaxAmmo = 50;

	BringUpTime = 0.41f;
	PutDownTime = 0.3f;
	WeaponBobScaling = 1.f;

	bFPFireFromCenter = true;
	FireOffset = FVector(75.0f, 0.0f, 0.0f);

	InactiveState = PCIP.CreateDefaultSubobject<UUTWeaponStateInactive>(this, TEXT("StateInactive"));
	ActiveState = PCIP.CreateDefaultSubobject<UUTWeaponStateActive>(this, TEXT("StateActive"));
	EquippingState = PCIP.CreateDefaultSubobject<UUTWeaponStateEquipping>(this, TEXT("StateEquipping"));
	UnequippingState = PCIP.CreateDefaultSubobject<UUTWeaponStateUnequipping>(this, TEXT("StateUnequipping"));

	Mesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Mesh1P"));
	Mesh->SetOnlyOwnerSee(true);
	Mesh->AttachParent = RootComponent;
	Mesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	FirstPMeshOffset = FVector(0.f);
	FirstPMeshRotation = FRotator(0.f, 0.f, 0.f);

	for (int32 i = 0; i < 2; i++)
	{
		UUTWeaponStateFiring* NewState = PCIP.CreateDefaultSubobject<UUTWeaponStateFiring, UUTWeaponStateFiring>(this, FName(*FString::Printf(TEXT("FiringState%i"), i)), false, false, false);
		if (NewState != NULL)
		{
			FiringState.Add(NewState);
#if WITH_EDITORONLY_DATA
			FiringStateType.Add(UUTWeaponStateFiring::StaticClass());
#endif
			FireInterval.Add(1.0f);
		}
	}

	RotChgSpeed = 3.f;
	ReturnChgSpeed = 3.f;
	MaxYawLag = 4.4f;
	MaxPitchLag = 3.3f; 

	// default icon texture
	static ConstructorHelpers::FObjectFinder<UTexture> WeaponTexture(TEXT("Texture2D'/Game/RestrictedAssets/Proto/UI/HUD/Elements/UI_HUD_BaseB.UI_HUD_BaseB'"));
	HUDIcon.Texture = WeaponTexture.Object;

	BaseAISelectRating = 0.55f;
}

void AUTWeapon::PostInitProperties()
{
	Super::PostInitProperties();

	DisplayName = FText::FromName(GetClass()->GetFName());
}

UMeshComponent* AUTWeapon::GetPickupMeshTemplate_Implementation(FVector& OverrideScale) const
{
	if (AttachmentType != NULL)
	{
		OverrideScale = AttachmentType.GetDefaultObject()->PickupScaleOverride;
		return AttachmentType.GetDefaultObject()->Mesh.Get();
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

#if WITH_EDITORONLY_DATA
void AUTWeapon::ValidateFiringStates()
{
	bool bMadeChanges = false;
	FiringState.SetNum(FiringStateType.Num());
	for (int32 i = 0; i < FiringStateType.Num(); i++)
	{
		// don't allow setting None
		EObjectFlags OldFlags = RF_NoFlags;
		if (FiringStateType[i] == NULL)
		{
			FiringStateType[i] = UUTWeaponStateFiring::StaticClass();
		}
		if (FiringState[i] != NULL && FiringState[i]->GetClass() != FiringStateType[i])
		{
			OldFlags = FiringState[i]->GetFlags();
			FiringState[i]->MarkPendingKill();
			FiringState[i] = NULL;
			bMadeChanges = true;
		}
		if (FiringState[i] == NULL && FiringStateType[i] != NULL)
		{
			FiringState[i] = ConstructObject<UUTWeaponStateFiring>(FiringStateType[i], this, NAME_None, OldFlags);
			bMadeChanges = true;
		}
	}
	if (bMadeChanges)
	{
		FEditorSupportDelegates::UpdateUI.Broadcast();
	}
}
void AUTWeapon::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property == NULL || PropertyChangedEvent.Property->GetFName() == FName(TEXT("FiringStateType")))
	{
		ValidateFiringStates();
	}
}
void AUTWeapon::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// we need to handle redirecting the component references when duplicating blueprints as this does not currently work correctly in engine
	// however, this is too early; the blueprint duplication and recompilation will clobber any changes
	// so we'll wait until the blueprint OnChanged event, which will work
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		UBlueprint* BP = Cast<UBlueprint>(GetClass()->ClassGeneratedBy);
		if (BP != NULL)
		{
			BP->OnChanged().RemoveUObject(AUTWeapon::StaticClass()->GetDefaultObject<AUTWeapon>(), &AUTWeapon::WeaponBPChanged); // make sure not already bound as there's an assert
			BP->OnChanged().AddUObject(AUTWeapon::StaticClass()->GetDefaultObject<AUTWeapon>(), &AUTWeapon::WeaponBPChanged);
		}
	}
}
void AUTWeapon::WeaponBPChanged(UBlueprint* BP)
{
	if (BP->GeneratedClass != NULL)
	{
		AUTWeapon* NewDefWeapon = BP->GeneratedClass->GetDefaultObject<AUTWeapon>();
		for (int32 i = 0; i < NewDefWeapon->MuzzleFlash.Num(); i++)
		{
			if (NewDefWeapon->MuzzleFlash[i] != NULL && !NewDefWeapon->MuzzleFlash[i]->IsIn(NewDefWeapon->GetClass()))
			{
				UParticleSystemComponent* NewMF = FindObject<UParticleSystemComponent>(NewDefWeapon->GetClass(), *NewDefWeapon->MuzzleFlash[i]->GetName(), false);
				if (NewMF != NULL)
				{
					NewDefWeapon->MuzzleFlash[i] = NewMF;
				}
			}
		}
	}
	BP->OnChanged().RemoveUObject(AUTWeapon::StaticClass()->GetDefaultObject<AUTWeapon>(), &AUTWeapon::WeaponBPChanged);
}
#endif

void AUTWeapon::BeginPlay()
{
#if WITH_EDITORONLY_DATA
	ValidateFiringStates();
#endif

	Super::BeginPlay();

	InstanceMuzzleFlashArray(this, MuzzleFlash);

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
	// make sure we initialized our state; this can be triggered if the weapon is spawned at game startup, since BeginPlay() will be deferred
	if (CurrentState == NULL)
	{
		GotoState(InactiveState);
	}

	Super::ClientGivenTo_Internal(bAutoActivate);

	// assign GroupSlot if required
	for (AUTInventory* Inv = UTOwner->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
	{
		if (Inv->GetOwner() == nullptr)
		{
			break;
		}

		if (Inv != this)
		{
			AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
			if (Weap != NULL && Weap->Group == Group)
			{
				GroupSlot = FMath::Max<int32>(GroupSlot, Weap->GroupSlot + 1);
			}
		}
	}

	if (bAutoActivate)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(UTOwner->Controller);
		if (PC != NULL)
		{
			PC->CheckAutoWeaponSwitch(this);
		}
	}
}

void AUTWeapon::Removed()
{
	GotoState(InactiveState);
	DetachFromOwner();

	Super::Removed();
}

void AUTWeapon::ClientRemoved_Implementation()
{
	GotoState(InactiveState);
	if (Role < ROLE_Authority) // already happened on authority in Removed()
	{
		DetachFromOwner();
	}

	AUTCharacter* OldOwner = UTOwner;

	Super::ClientRemoved_Implementation();

	if (OldOwner != NULL && OldOwner->GetWeapon() == this)
	{
		OldOwner->ClientWeaponLost(this);
	}
}

float AUTWeapon::GetAutoSwitchPriority()
{
	return (AutoSwitchPriority > 0.0f) ? AutoSwitchPriority : (float(Group) + float(GroupSlot) * 0.01f);
}

void AUTWeapon::StartFire(uint8 FireModeNum)
{
	if (!UTOwner->bDisallowWeaponFiring)
	{
		BeginFiringSequence(FireModeNum);
		if (Role < ROLE_Authority)
		{
			ServerStartFire(FireModeNum);
		}
	}
}

void AUTWeapon::ServerStartFire_Implementation(uint8 FireModeNum)
{
	if (!UTOwner->bDisallowWeaponFiring)
	{
		BeginFiringSequence(FireModeNum);
	}
}

bool AUTWeapon::ServerStartFire_Validate(uint8 FireModeNum)
{
	return true;
}

void AUTWeapon::BeginFiringSequence(uint8 FireModeNum)
{
	UTOwner->SetPendingFire(FireModeNum, true);
	if (FiringState.IsValidIndex(FireModeNum))
	{
		FiringState[FireModeNum]->PendingFireStarted();
	}
	CurrentState->BeginFiringSequence(FireModeNum);
	if (CurrentState->IsFiring() && CurrentFireMode != FireModeNum)
	{
		OnMultiPress(FireModeNum);
	}
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
	UTOwner->SetPendingFire(FireModeNum, false);
	if (FiringState.IsValidIndex(FireModeNum))
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

bool AUTWeapon::PutDown()
{
	if (eventPreventPutDown())
	{
		return false;
	}
	else
	{
		CurrentState->PutDown();
		return true;
	}
}

void AUTWeapon::UnEquip()
{
	GotoState(UnequippingState);
}

void AUTWeapon::AttachToOwner_Implementation()
{
	if (UTOwner == NULL)
	{
		return;
	}

	// sanity check some settings
	for (int32 i = 0; i < MuzzleFlash.Num(); i++)
	{
		if (MuzzleFlash[i] != NULL)
		{
			MuzzleFlash[i]->bAutoActivate = false;
			MuzzleFlash[i]->SetOnlyOwnerSee(true);
		}
	}
	// attach
	if (Mesh != NULL && Mesh->SkeletalMesh != NULL)
	{
		Mesh->AttachTo(UTOwner->FirstPersonMesh);
		if (Cast<APlayerController>(UTOwner->Controller) != NULL && UTOwner->IsLocallyControlled())
		{
			Mesh->LastRenderTime = GetWorld()->TimeSeconds;
			Mesh->bRecentlyRendered = true;
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

void AUTWeapon::DetachFromOwner_Implementation()
{
	for (int32 i = 0; i < FiringState.Num(); i++)
	{
		FiringState[i]->WeaponBecameInactive();
	}
	if (Mesh != NULL && Mesh->SkeletalMesh != NULL)
	{
		Mesh->DetachFromParent();
	}
	// unregister components so they go away
	UnregisterAllComponents();
}

void AUTWeapon::PlayFiringEffects()
{
	if (UTOwner != NULL)
	{
		// try and play the sound if specified
		if (FireSound.IsValidIndex(CurrentFireMode) && FireSound[CurrentFireMode] != NULL)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), FireSound[CurrentFireMode], UTOwner, SRT_AllButOwner);
		}

		if (GetNetMode() != NM_DedicatedServer)
		{
			// try and play a firing animation if specified
			if (FireAnimation.IsValidIndex(CurrentFireMode) && FireAnimation[CurrentFireMode] != NULL)
			{
				UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
				if (AnimInstance != NULL)
				{
					AnimInstance->Montage_Play(FireAnimation[CurrentFireMode], UTOwner->GetFireRateMultiplier());
				}
			}

			// muzzle flash
			if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL && MuzzleFlash[CurrentFireMode]->Template != NULL)
			{
				// if we detect a looping particle system, then don't reactivate it
				if (!MuzzleFlash[CurrentFireMode]->bIsActive || MuzzleFlash[CurrentFireMode]->bSuppressSpawning || !IsLoopingParticleSystem(MuzzleFlash[CurrentFireMode]->Template))
				{
					MuzzleFlash[CurrentFireMode]->ActivateSystem();
				}
			}
		}
	}
}
void AUTWeapon::StopFiringEffects()
{
	if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL)
	{
		MuzzleFlash[CurrentFireMode]->DeactivateSystem();
	}
}

FHitResult AUTWeapon::GetImpactEffectHit(APawn* Shooter, const FVector& StartLoc, const FVector& TargetLoc)
{
	// trace for precise hit location and hit normal
	FHitResult Hit;
	FVector TargetToGun = (StartLoc - TargetLoc).SafeNormal();
	if (Shooter->GetWorld()->LineTraceSingle(Hit, TargetLoc + TargetToGun * 10.0f, TargetLoc - TargetToGun * 10.0f, COLLISION_TRACE_WEAPON, FCollisionQueryParams(FName(TEXT("ImpactEffect")), false, Shooter)))
	{
		return Hit;
	}
	else
	{
		return FHitResult(NULL, NULL, TargetLoc, TargetToGun);
	}
}

void AUTWeapon::PlayImpactEffects(const FVector& TargetLoc)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		// fire effects
		static FName NAME_HitLocation(TEXT("HitLocation"));
		static FName NAME_LocalHitLocation(TEXT("LocalHitLocation"));
		const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL) ? MuzzleFlash[CurrentFireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetControlRotation().RotateVector(FireOffset);
		if (FireEffect.IsValidIndex(CurrentFireMode) && FireEffect[CurrentFireMode] != NULL)
		{
			UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[CurrentFireMode], SpawnLocation, (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL) ? MuzzleFlash[CurrentFireMode]->GetComponentRotation() : (TargetLoc - SpawnLocation).Rotation(), true);
			PSC->SetVectorParameter(NAME_HitLocation, TargetLoc);
			PSC->SetVectorParameter(NAME_LocalHitLocation, PSC->ComponentToWorld.InverseTransformPosition(TargetLoc));
		}
		// perhaps the muzzle flash also contains hit effect (constant beam, etc) so set the parameter on it instead
		else if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL)
		{
			MuzzleFlash[CurrentFireMode]->SetVectorParameter(NAME_HitLocation, TargetLoc);
			MuzzleFlash[CurrentFireMode]->SetVectorParameter(NAME_LocalHitLocation, MuzzleFlash[CurrentFireMode]->ComponentToWorld.InverseTransformPositionNoScale(TargetLoc));
		}

		if ((TargetLoc - LastImpactEffectLocation).Size() >= ImpactEffectSkipDistance || GetWorld()->TimeSeconds - LastImpactEffectTime >= MaxImpactEffectSkipTime)
		{
			if (ImpactEffect.IsValidIndex(CurrentFireMode) && ImpactEffect[CurrentFireMode] != NULL)
			{
				FHitResult ImpactHit = GetImpactEffectHit(UTOwner, SpawnLocation, TargetLoc);
				ImpactEffect[CurrentFireMode].GetDefaultObject()->SpawnEffect(GetWorld(), FTransform(ImpactHit.Normal.Rotation(), ImpactHit.Location), ImpactHit.Component.Get(), NULL, UTOwner->Controller);
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
		static FName NAME_FiredWeapon(TEXT("FiredWeapon"));
		GetUTOwner()->InventoryEvent(NAME_FiredWeapon);
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
			UE_LOG(UT, Warning, TEXT("********** %s ran out of ammo for %s"), *GetName(), *PC->GetHumanReadableName());
			PC->SwitchToBestWeapon();
		}
	}
}

void AUTWeapon::ConsumeAmmo(uint8 FireModeNum)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// cheat
	if (UTOwner && UTOwner->bUnlimitedAmmo)
	{
		return;
	}
#endif

	if (Role == ROLE_Authority)
	{
		if (AmmoCost.IsValidIndex(FireModeNum))
		{
			AddAmmo(-AmmoCost[FireModeNum]);
		}
		else
		{
			UE_LOG(UT, Warning, TEXT("Invalid fire mode %i in %s::ConsumeAmmo()"), int32(FireModeNum), *GetName());
		}
	}
}

bool AUTWeapon::HasAmmo(uint8 FireModeNum)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// cheat
	if (UTOwner && UTOwner->bUnlimitedAmmo)
	{
		return true;
	}
#endif

	return (AmmoCost.IsValidIndex(FireModeNum) && Ammo >= AmmoCost[FireModeNum]);
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

FVector AUTWeapon::GetFireStartLoc()
{
	if (UTOwner == NULL)
	{
		UE_LOG(UT, Warning, TEXT("%s::GetFireStartLoc(): No Owner (died while firing?)"), *GetName());
		return FVector::ZeroVector;
	}
	else
	{
		FVector BaseLoc;
		if (bFPFireFromCenter && Cast<APlayerController>(UTOwner->Controller) != NULL) // TODO: first person view check
		{
			BaseLoc = UTOwner->GetPawnViewLocation();
		}
		else
		{
			BaseLoc = UTOwner->GetActorLocation();
		}
		if (FireOffset.IsZero())
		{
			return BaseLoc;
		}
		else
		{
			// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
			FVector FinalLoc = BaseLoc + GetBaseFireRotation().RotateVector(FireOffset);
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
			{
				FHitResult Hit;
				if (UTOwner->CapsuleComponent->SweepComponent(Hit, FinalLoc, BaseLoc, Collider, false))
				{
					BaseLoc = Hit.Location;
				}
			}
			FCollisionQueryParams Params(FName(TEXT("WeaponStartLoc")), false, UTOwner);
			FHitResult Hit;
			if (GetWorld()->SweepSingle(Hit, BaseLoc, FinalLoc, FQuat::Identity, COLLISION_TRACE_WEAPON, Collider, Params))
			{
				FinalLoc = Hit.Location - (FinalLoc - BaseLoc).SafeNormal();
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
	else
	{
		return UTOwner->GetViewRotation();
	}
}

FRotator AUTWeapon::GetAdjustedAim_Implementation(FVector StartFireLoc)
{
	FRotator BaseAim = GetBaseFireRotation();
	if (Spread.IsValidIndex(CurrentFireMode) && Spread[CurrentFireMode] > 0.0f)
	{
		// add in any spread
		FRotationMatrix Mat(BaseAim);
		FVector X, Y, Z;
		Mat.GetScaledAxes(X, Y, Z);
		float RandY = 0.5f * (FMath::FRand() + FMath::FRand() - 1.f);
		float RandZ = FMath::Sqrt(0.25f - FMath::Square(RandY)) * (FMath::FRand() + FMath::FRand() - 1.f);
		return (X + RandY * Spread[CurrentFireMode] * Y + RandZ * Spread[CurrentFireMode] * Z).Rotation();
	}
	else
	{
		return BaseAim;
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
	if (!GetWorld()->LineTraceSingle(Hit, SpawnLocation, EndTrace, COLLISION_TRACE_WEAPON, FCollisionQueryParams(GetClass()->GetFName(), false, UTOwner)))
	{
		Hit.Location = EndTrace;
	}
	AUTPlayerController* UTPC = UTOwner ? Cast<AUTPlayerController>(UTOwner->Controller) : NULL;
	if (Role == ROLE_Authority)
	{
		UTOwner->SetFlashLocation(Hit.Location, CurrentFireMode);
	}
	else if (UTPC && (UTPC->GetPredictionTime() > 0.f))
	{
		UTOwner->SetFlashLocation(Hit.Location, CurrentFireMode);
		PlayImpactEffects(Hit.Location);
	}
	if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged && bDealDamage)
	{
		Hit.Actor->TakeDamage(InstantHitInfo[CurrentFireMode].Damage, FUTPointDamageEvent(InstantHitInfo[CurrentFireMode].Damage, Hit, FireDir, InstantHitInfo[CurrentFireMode].DamageType, FireDir * GetImpartedMomentumMag(Hit.Actor.Get())), UTOwner->Controller, this);
	}
	if (OutHit != NULL)
	{
		*OutHit = Hit;
	}
}

float AUTWeapon::GetImpartedMomentumMag(AActor* HitActor)
{
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

	if ((CatchupTickDelta > 0.f) && (Role != ROLE_Authority))
	{
		// lag is so high need to delay spawn
		float SleepTime = OwningPlayer->GetProjectileSleepTime();
		if (SleepTime > 0.f)
		{
			if (!GetWorldTimerManager().IsTimerActive(this, &AUTWeapon::SpawnDelayedFakeProjectile))
			{
				DelayedProjectile.ProjectileClass = ProjectileClass;
				DelayedProjectile.SpawnLocation = SpawnLocation;
				DelayedProjectile.SpawnRotation = SpawnRotation;
				GetWorldTimerManager().SetTimer(this, &AUTWeapon::SpawnDelayedFakeProjectile, SleepTime, false);
			}
			return NULL;
		}
	}
	FActorSpawnParameters Params;
	Params.Instigator = UTOwner;
	Params.Owner = UTOwner;
	AUTProjectile* NewProjectile = 
		((Role == ROLE_Authority) || (CatchupTickDelta > 0.f))
		? GetWorld()->SpawnActor<AUTProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, Params)
		: NULL;
	if (NewProjectile)
	{
		if (Role == ROLE_Authority)
		{
			if ((CatchupTickDelta > 0.f) && NewProjectile->ProjectileMovement)
			{
				// server ticks projectile to match with when client actually fired
				NewProjectile->ProjectileMovement->TickComponent(CatchupTickDelta, LEVELTICK_All, NULL);
			}
		}
		else
		{
			NewProjectile->InitFakeProjectile(OwningPlayer);
		}
	}
	return NewProjectile;
}

void AUTWeapon::SpawnDelayedFakeProjectile()
{
	FActorSpawnParameters Params;
	Params.Instigator = UTOwner;
	Params.Owner = UTOwner;
	AUTProjectile* NewProjectile = GetWorld()->SpawnActor<AUTProjectile>(DelayedProjectile.ProjectileClass, DelayedProjectile.SpawnLocation, DelayedProjectile.SpawnRotation, Params);
	if (NewProjectile)
	{
		AUTPlayerController* OwningPlayer = UTOwner ? Cast<AUTPlayerController>(UTOwner->GetController()) : NULL;
		NewProjectile->InitFakeProjectile(OwningPlayer);
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
	return true;
}

float AUTWeapon::LagWeaponRotation(float NewValue, float LastValue, float DeltaTime, float MaxDiff, int Index)
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

		// if weapon is up in first person, view bob with movement
		if (Mesh != NULL && UTOwner != NULL && UTOwner->IsLocallyControlled())
		{
			if (FirstPMeshOffset.IsZero())
			{
				FirstPMeshOffset = Mesh->GetRelativeTransform().GetLocation();
				FirstPMeshRotation = Mesh->GetRelativeTransform().Rotator();
			}
			Mesh->SetRelativeLocation(FirstPMeshOffset);
			Mesh->SetWorldLocation(Mesh->GetComponentLocation() + UTOwner->GetWeaponBobOffset(DeltaTime, this));

			FRotator NewRotation = UTOwner ? UTOwner->GetControlRotation() : FRotator(0.f,0.f,0.f);
			FRotator FinalRotation = NewRotation;

			// Add some rotation leading
			if (UTOwner && UTOwner->Controller)
			{
				FinalRotation.Yaw = LagWeaponRotation(NewRotation.Yaw, LastRotation.Yaw, DeltaTime, MaxYawLag, 0);
				FinalRotation.Pitch = LagWeaponRotation(NewRotation.Pitch, LastRotation.Pitch, DeltaTime, MaxPitchLag, 1);
				FinalRotation.Roll = NewRotation.Roll;
			}
			LastRotation = NewRotation;
			Mesh->SetRelativeRotation(FinalRotation + FirstPMeshRotation);
		}
	}
}

void AUTWeapon::Destroyed()
{
	Super::Destroyed();

	// this makes sure timers, etc go away
	GotoState(InactiveState);
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
	DOREPLIFETIME_CONDITION(AUTWeapon, Ammo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTWeapon, MaxAmmo, COND_OwnerOnly);
}

FLinearColor AUTWeapon::GetCrosshairColor(UUTHUDWidget* WeaponHudWidget) const
{
	FLinearColor CrosshairColor(FLinearColor::White);
	float TimeSinceHit = GetWorld()->TimeSeconds - WeaponHudWidget->UTHUDOwner->LastConfirmedHitTime;
	if (TimeSinceHit < 0.4f)
	{
		CrosshairColor = FMath::Lerp<FLinearColor>(FLinearColor::Red, CrosshairColor, FMath::Lerp<float>(0.f, 1.f, FMath::Pow((GetWorld()->TimeSeconds - WeaponHudWidget->UTHUDOwner->LastConfirmedHitTime) / 0.4f, 2.0f)));
	}
	return CrosshairColor;
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
		GetWorld()->LineTraceSingle(Hit, CameraLoc, CameraLoc + CameraRot.Vector() * 50000.0f, COLLISION_TRACE_WEAPON, FCollisionQueryParams(FName(TEXT("CrosshairFriendIndicator")), false, UTOwner));
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

void AUTWeapon::DrawWeaponCrosshair_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta)
{
	bool bDrawCrosshair = true;
	for (int32 i = 0; i < FiringState.Num(); i++)
	{
		bDrawCrosshair = FiringState[i]->DrawHUD(WeaponHudWidget) && bDrawCrosshair;
	}

	if (bDrawCrosshair)
	{
		UTexture2D* CrosshairTexture = WeaponHudWidget->UTHUDOwner->DefaultCrosshairTex;
		if (CrosshairTexture != NULL)
		{
			float W = CrosshairTexture->GetSurfaceWidth();
			float H = CrosshairTexture->GetSurfaceHeight();

			float Scale = WeaponHudWidget->GetRenderScale();
			
			// draw a different indicator if there is a friendly where the camera is pointing
			AUTPlayerState* PS;
			if (ShouldDrawFFIndicator(WeaponHudWidget->UTHUDOwner->PlayerOwner, PS))
			{
				WeaponHudWidget->DrawTexture(CrosshairTexture, 0, 0, W * Scale * 2.0f, H * Scale * 2.0f, 0.0, 0.0, 16, 16, 1.0, FLinearColor::Green, FVector2D(0.5f, 0.5f), 45.0f);
			}
			else
			{
				WeaponHudWidget->DrawTexture(CrosshairTexture, 0, 0, W * Scale, H * Scale, 0.0, 0.0, 16, 16, 1.0, GetCrosshairColor(WeaponHudWidget), FVector2D(0.5f, 0.5f));
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

void AUTWeapon::DrawWeaponInfo_Implementation(UUTHUDWidget* WeaponHudWidget, float RenderDelta)
{
	UFont* Font = WeaponHudWidget->UTHUDOwner->MediumFont;
	FString AmmoStr = FString::Printf(TEXT("%i"),Ammo);
	FText AmmoText = FText::FromString( AmmoStr );
	WeaponHudWidget->DrawText(AmmoText,0,0,Font, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Bottom);
}

void AUTWeapon::UpdateOverlaysShared(AActor* WeaponActor, AUTCharacter* InOwner, USkeletalMeshComponent* InMesh, USkeletalMeshComponent*& InOverlayMesh) const
{
	AUTGameState* GS = WeaponActor->GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		UMaterialInterface* TopOverlay = GS->GetFirstOverlay(InOwner->GetWeaponOverlayFlags());
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
	if (NewSkin != NULL)
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, NewSkin);
		}
	}
	else
	{
		for (int32 i = 0; i < Mesh->GetNumMaterials(); i++)
		{
			Mesh->SetMaterial(i, GetClass()->GetDefaultObject<AUTWeapon>()->Mesh->GetMaterial(i));
		}
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
			if (AlreadyHas->Ammo >= AlreadyHas->MaxAmmo)
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

float AUTWeapon::GetAISelectRating_Implementation()
{
	return HasAnyAmmo() ? BaseAISelectRating : 0.0f;
}
