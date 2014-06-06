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

AUTWeapon::AUTWeapon(const FPostConstructInitializeProperties& PCIP)
: Super(PCIP.DoNotCreateDefaultSubobject(TEXT("PickupMesh")))
{
	AmmoCost.Add(1);
	AmmoCost.Add(1);

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;

	bWeaponStay = true;

	Ammo = 20;
	MaxAmmo = 50;

	BringUpTime = 0.5f;
	PutDownTime = 0.5f;

	bFPFireFromCenter = true;
	FireOffset = FVector(75.0f, 0.0f, 0.0f);

	InactiveState = PCIP.CreateDefaultSubobject<UUTWeaponStateInactive>(this, TEXT("StateInactive"));
	ActiveState = PCIP.CreateDefaultSubobject<UUTWeaponStateActive>(this, TEXT("StateActive"));
	EquippingState = PCIP.CreateDefaultSubobject<UUTWeaponStateEquipping>(this, TEXT("StateEquipping"));
	UnequippingState = PCIP.CreateDefaultSubobject<UUTWeaponStateUnequipping>(this, TEXT("StateUnequipping"));

	Mesh = PCIP.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("Mesh1P"));
	Mesh->SetOnlyOwnerSee(true);
	Mesh->AttachParent = RootComponent;

	for (int32 i = 0; i < 2; i++)
	{
		UUTWeaponStateFiring* NewState = PCIP.CreateDefaultSubobject<UUTWeaponStateFiring, UUTWeaponStateFiring>(this, FName(*FString::Printf(TEXT("FiringState%i"), i)), false, false, false);
		if (NewState != NULL)
		{
			FiringState.Add(NewState);
#if WITH_EDITOR
			FiringStateType.Add(UUTWeaponStateFiring::StaticClass());
#endif
			FireInterval.Add(1.0f);
		}
	}
}

UMeshComponent* AUTWeapon::GetPickupMeshTemplate_Implementation()
{
	return (AttachmentType != NULL) ? AttachmentType.GetDefaultObject()->Mesh.Get() : Super::GetPickupMeshTemplate_Implementation();
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
					if (ConstructionNodes[j]->ComponentTemplate == MFArray[k])
					{
						MFArray[k] = Cast<UParticleSystemComponent>((UObject*)FindObjectWithOuter(Weap, ConstructionNodes[j]->ComponentTemplate->GetClass(), ConstructionNodes[j]->VariableName));
					}
				}
			}
		}
	}
}

void AUTWeapon::ValidateFiringStates()
{
#if WITH_EDITORONLY_DATA
	bool bMadeChanges = false;
	FiringState.SetNum(FiringStateType.Num());
	for (int32 i = 0; i < FiringStateType.Num(); i++)
	{
		// don't allow setting None
		if (FiringStateType[i] == NULL)
		{
			FiringStateType[i] = UUTWeaponStateFiring::StaticClass();
		}
		if (FiringState[i] != NULL && FiringState[i]->GetClass() != FiringStateType[i])
		{
			FiringState[i]->MarkPendingKill();
			FiringState[i] = NULL;
			bMadeChanges = true;
		}
		if (FiringState[i] == NULL && FiringStateType[i] != NULL)
		{
			FiringState[i] = ConstructObject<UUTWeaponStateFiring>(FiringStateType[i], this);
			bMadeChanges = true;
		}
	}
	if (bMadeChanges)
	{
		FEditorSupportDelegates::UpdateUI.Broadcast();
	}
#endif
}
#if WITH_EDITORONLY_DATA
void AUTWeapon::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property == NULL || PropertyChangedEvent.Property->GetFName() == FName(TEXT("FiringStateType")))
	{
		ValidateFiringStates();
	}
}
#endif

void AUTWeapon::BeginPlay()
{
	// FIXME: temp until 4.2 merge
	ValidateFiringStates();

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
	else if (CurrentState != NewState)
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
		}
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
		if (Inv != this)
		{
			AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
			if (Weap != NULL && Weap->Group == Group)
			{
				Group = FMath::Max<int32>(Group, Weap->Group + 1);
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

	Super::Removed();
}
void AUTWeapon::ClientRemoved_Implementation()
{
	GotoState(InactiveState);

	Super::ClientRemoved_Implementation();
}

void AUTWeapon::StartFire(uint8 FireModeNum)
{
	BeginFiringSequence(FireModeNum);
	if (Role < ROLE_Authority)
	{
		ServerStartFire(FireModeNum);
	}
}
void AUTWeapon::ServerStartFire_Implementation(uint8 FireModeNum)
{
	BeginFiringSequence(FireModeNum);
}
bool AUTWeapon::ServerStartFire_Validate(uint8 FireModeNum)
{
	return true;
}
void AUTWeapon::BeginFiringSequence(uint8 FireModeNum)
{
	UTOwner->SetPendingFire(FireModeNum, true);
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
	CurrentState->EndFiringSequence(FireModeNum);
}

void AUTWeapon::BringUp()
{
	AttachToOwner();

	CurrentState->BringUp();
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

void AUTWeapon::AttachToOwner_Implementation()
{
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
	}
	// register components now
	Super::RegisterAllComponents();
}

void AUTWeapon::DetachFromOwner_Implementation()
{
	if (Mesh != NULL && Mesh->SkeletalMesh != NULL)
	{
		Mesh->DetachFromParent();
	}
	// unregister components so they go away
	UnregisterAllComponents();
}

void AUTWeapon::PlayFiringEffects()
{
	if (GetNetMode() != NM_DedicatedServer && UTOwner != NULL)
	{
		// try and play the sound if specified
		if (FireSound.IsValidIndex(CurrentFireMode) && FireSound[CurrentFireMode] != NULL)
		{
			UUTGameplayStatics::UTPlaySound(GetWorld(), FireSound[CurrentFireMode], UTOwner, SRT_AllButOwner);
		}

		// try and play a firing animation if specified
		if (FireAnimation.IsValidIndex(CurrentFireMode) && FireAnimation[CurrentFireMode] != NULL)
		{
			UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
			if (AnimInstance != NULL)
			{
				AnimInstance->Montage_Play(FireAnimation[CurrentFireMode], 1.f);
			}
		}

		// muzzle flash
		if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL && MuzzleFlash[CurrentFireMode]->Template != NULL)
		{
			// if we detect a looping particle system, then don't reactivate it
			if ( !MuzzleFlash[CurrentFireMode]->bIsActive || MuzzleFlash[CurrentFireMode]->Template->Emitters[0] == NULL ||
				MuzzleFlash[CurrentFireMode]->Template->Emitters[0]->GetLODLevel(0)->RequiredModule->EmitterLoops > 0 )
			{
				MuzzleFlash[CurrentFireMode]->ActivateSystem();
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
void AUTWeapon::PlayImpactEffects(const FVector& TargetLoc)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		// fire effects
		static FName NAME_HitLocation(TEXT("HitLocation"));
		if (FireEffect.IsValidIndex(CurrentFireMode) && FireEffect[CurrentFireMode] != NULL)
		{
			const FVector SpawnLocation = (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL) ? MuzzleFlash[CurrentFireMode]->GetComponentLocation() : UTOwner->GetActorLocation() + UTOwner->GetControlRotation().RotateVector(FireOffset);
			UParticleSystemComponent* PSC = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), FireEffect[CurrentFireMode], SpawnLocation, (TargetLoc - SpawnLocation).Rotation(), true);
			PSC->SetVectorParameter(NAME_HitLocation, TargetLoc);
		}
		// perhaps the muzzle flash also contains hit effect (constant beam, etc) so set the parameter on it instead
		else if (MuzzleFlash.IsValidIndex(CurrentFireMode) && MuzzleFlash[CurrentFireMode] != NULL)
		{
			MuzzleFlash[CurrentFireMode]->SetVectorParameter(NAME_HitLocation, TargetLoc);
		}
	}
}

void AUTWeapon::FireShot()
{
	ConsumeAmmo(CurrentFireMode);

	if (!FireShotOverride())
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
}

bool AUTWeapon::IsFiring() const
{
	return CurrentState->IsFiring();
}

void AUTWeapon::AddAmmo(int32 Amount)
{
	Ammo = FMath::Clamp<int32>(Ammo + Amount, 0, MaxAmmo);
	
	// trigger weapon switch if necessary
	if (UTOwner != NULL && UTOwner->IsLocallyControlled())
	{
		OnRep_Ammo();
	}
}

void AUTWeapon::OnRep_Ammo()
{
	for (int32 i = GetNumFireModes() - 1; i >= 0; i--)
	{
		if (HasAmmo(i))
		{
			return;
		}
	}
	if (UTOwner != NULL)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(UTOwner->Controller);
		if (PC != NULL)
		{
			PC->SwitchToBestWeapon();
		}
	}
}

void AUTWeapon::ConsumeAmmo(uint8 FireModeNum)
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

bool AUTWeapon::HasAmmo(uint8 FireModeNum)
{
	return (AmmoCost.IsValidIndex(FireModeNum) && Ammo > AmmoCost[FireModeNum]);
}

FVector AUTWeapon::GetFireStartLoc()
{
	if (UTOwner == NULL)
	{
		UE_LOG(UT, Warning, TEXT("GetFireStartLoc(): No Owner (died while firing?)"));
		return FVector::ZeroVector;
	}
	else if (bFPFireFromCenter && Cast<APlayerController>(UTOwner->Controller) != NULL) // TODO: first person view check
	{
		return UTOwner->GetPawnViewLocation() + GetBaseFireRotation().RotateVector(FireOffset);
	}
	else
	{
		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
		return UTOwner->GetActorLocation() + GetBaseFireRotation().RotateVector(FireOffset);
	}
}

FRotator AUTWeapon::GetBaseFireRotation()
{
	if (UTOwner == NULL)
	{
		UE_LOG(UT, Warning, TEXT("GetBaseFireRotation(): No Owner (died while firing?)"));
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
		float RandY = FMath::FRand() - 0.5f;
		float RandZ = FMath::Sqrt(0.5f - FMath::Square(RandY)) * (FMath::FRand() - 0.5f);
		return (X + RandY * Spread[CurrentFireMode] * Y + RandZ * Spread[CurrentFireMode] * Z).Rotation();
	}
	else
	{
		return BaseAim;
	}
}

void AUTWeapon::FireInstantHit(bool bDealDamage, FHitResult* OutHit)
{
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
	if (Role == ROLE_Authority)
	{
		UTOwner->SetFlashLocation(Hit.Location, CurrentFireMode);
	}
	if (Hit.Actor != NULL && Hit.Actor->bCanBeDamaged && bDealDamage)
	{
		Hit.Actor->TakeDamage(InstantHitInfo[CurrentFireMode].Damage, FUTPointDamageEvent(InstantHitInfo[CurrentFireMode].Damage, Hit, FireDir, InstantHitInfo[CurrentFireMode].DamageType, FireDir * InstantHitInfo[CurrentFireMode].Momentum), UTOwner->Controller, this);
	}
	if (OutHit != NULL)
	{
		*OutHit = Hit;
	}
}
void AUTWeapon::K2_FireInstantHit(bool bDealDamage, FHitResult& OutHit)
{
	FireInstantHit(bDealDamage, &OutHit);
}

AUTProjectile* AUTWeapon::FireProjectile()
{
	if (Role == ROLE_Authority)
	{
		checkSlow(ProjClass.IsValidIndex(CurrentFireMode) && ProjClass[CurrentFireMode] != NULL);

		// try and fire a projectile
		const FVector SpawnLocation = GetFireStartLoc();
		const FRotator SpawnRotation = GetAdjustedAim(SpawnLocation);

		UTOwner->IncrementFlashCount(CurrentFireMode);

		// spawn the projectile at the muzzle
		FActorSpawnParameters Params;
		Params.Instigator = UTOwner;
		Params.Owner = UTOwner;
		return GetWorld()->SpawnActor<AUTProjectile>(ProjClass[CurrentFireMode], SpawnLocation, SpawnRotation, Params);
	}
	else
	{
		return NULL;
	}
}

float AUTWeapon::GetRefireTime(uint8 FireModeNum)
{
	if (FireInterval.IsValidIndex(FireModeNum))
	{
		float Result = FireInterval[FireModeNum];
		if (UTOwner != NULL)
		{
			Result *= UTOwner->GetFireRateMultiplier();
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

void AUTWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// apparently things can get ticked before BeginPlay() is called. Seems like bad news...
	if (CurrentState != NULL && CurrentState != InactiveState)
	{
		CurrentState->Tick(DeltaTime);
	}
}

void AUTWeapon::Destroyed()
{
	Super::Destroyed();

	// this makes sure timers, etc go away
	GotoState(InactiveState);
}

void AUTWeapon::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AUTWeapon, Ammo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTWeapon, MaxAmmo, COND_OwnerOnly);
}

void AUTWeapon::DrawHud_Implementation(AUTHUD* Hud, UCanvas* Canvas)
{
	const FVector2D CrossHairCenter(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// Draw very simple crosshair

	// offset by half the texture's dimensions so that the center of the texture aligns with the center of the Canvas
	const FVector2D CrosshairDrawPosition( (CrossHairCenter.X - (Hud->DefaultCrosshairTex->GetSurfaceWidth() * 0.5f)),
											(CrossHairCenter.Y - (Hud->DefaultCrosshairTex->GetSurfaceHeight() * 0.5f)) );

	// draw the crosshair
	FCanvasTileItem TileItem( CrosshairDrawPosition, Hud->DefaultCrosshairTex->Resource, FLinearColor::White);
	TileItem.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem( TileItem );
}

void AUTWeapon::DrawWeaponInfo(UUTHUDWidget* WeaponHudWidget, float RenderDelta)
{
	UFont* Font = WeaponHudWidget->UTHUDOwner->MediumFont;
	FString AmmoStr = FString::Printf(TEXT("%i"),Ammo);
	FText AmmoText = FText::FromString( AmmoStr );
	WeaponHudWidget->DrawText(AmmoText,0,0,Font, 1.0f, 1.0f, FLinearColor::White, ETextHorzPos::Right, ETextVertPos::Bottom);
}
