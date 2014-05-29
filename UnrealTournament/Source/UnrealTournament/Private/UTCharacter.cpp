// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTCharacterMovement.h"
#include "UTProjectile.h"
#include "UTWeaponAttachment.h"
#include "UnrealNetwork.h"

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
	FirstPersonMesh->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	FirstPersonMesh->AttachParent = CharacterCameraComponent;
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;

	HealthMax = 100;
}

void AUTCharacter::BeginPlay()
{
	if (Health == 0)
	{
		Health = HealthMax;
	}
	Super::BeginPlay();
}

void AUTCharacter::Restart()
{
	Super::Restart();
	ClearJumpInput();
}

void AUTCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// TODO: should be done by game type
	AddInventory(GetWorld()->SpawnActor<AUTWeapon>(DefaultWeapon, FVector(0.0f), FRotator(0, 0, 0)), true);
}

float AUTCharacter::TakeDamage(float Damage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (IsDead())
	{
		// already dead
		// TODO: ragdoll impulses, gibbing, etc
		return Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	}
	else
	{
		int32 ResultDamage = FMath::Trunc(Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser));
		// TODO: gametype links, etc

		Health -= ResultDamage;
		SetLastTakeHitInfo(ResultDamage, DamageEvent);
		if (Health <= 0)
		{
			Died(EventInstigator, DamageEvent);
		}

		return float(ResultDamage);
	}
}

void AUTCharacter::SetLastTakeHitInfo(int32 Damage, const FDamageEvent& DamageEvent)
{
	LastTakeHitInfo.Damage = Damage;
	LastTakeHitInfo.DamageType = DamageEvent.DamageTypeClass;
	LastTakeHitInfo.Momentum = FVector::ZeroVector; // TODO

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
	if (GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodEffect, LastTakeHitInfo.RelHitLocation + GetActorLocation(), LastTakeHitInfo.RelHitLocation.Rotation());
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
	// TODO: ragdoll, et al
	//		also remember need to do lifespan for replication, not straight destroy
	Destroy();
}

void AUTCharacter::Destroyed()
{
	Super::Destroyed();

	DiscardAllInventory();
	if (WeaponAttachment != NULL)
	{
		WeaponAttachment->Destroy();
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
				if (!bTearOff)
				{
					AUTPlayerController* PC = Cast<AUTPlayerController>(Controller);
					if (PC != NULL)
					{
						PC->SwitchToBestWeapon();
					}
				}
				if (Weapon == NULL)
				{
					WeaponClass = NULL;
					UpdateWeaponAttachment();
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

void AUTCharacter::TossInventory(AUTInventory* InvToToss)
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
		InvToToss->DropFrom(GetActorLocation() + GetActorRotation().Vector() * GetSimpleCollisionCylinderExtent().X, GetVelocity() + GetActorRotation().RotateVector(FVector(300.0f, 0.0f, 150.0f)));
	}
}

void AUTCharacter::DiscardAllInventory()
{
	while (InventoryList != NULL)
	{
		InventoryList->Destroy();
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

void AUTCharacter::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
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

bool AUTCharacter::Dodge(const FVector &DodgeDir, const FVector &DodgeCross)
{
	if (CanDodge())
	{
		if ( DodgeOverride(DodgeDir, DodgeCross) )
		{
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

bool AUTCharacter::CanJump() const
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
	}
}



