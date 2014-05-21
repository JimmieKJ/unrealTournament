// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacter.h"
#include "UTProjectile.h"
#include "UnrealNetwork.h"

//////////////////////////////////////////////////////////////////////////
// AUTCharacter

AUTCharacter::AUTCharacter(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// Set size for collision capsule
	CapsuleComponent->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

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

		GetWorld()->GetAuthGameMode<AUTGameMode>()->Killed(EventInstigator, (Controller != NULL) ? Controller : Cast<AController>(GetOwner()), this, DamageEvent.DamageTypeClass);

		Health = FMath::Min<int32>(Health, 0);
		if (Controller != NULL)
		{
			Controller->PawnPendingDestroy(this);
		}

		bTearOff = true;
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

//////////////////////////////////////////////////////////////////////////
// Input

void AUTCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	// set up gameplay key bindings
	check(InputComponent);

	InputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);

	InputComponent->BindAxis("MoveForward", this, &AUTCharacter::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AUTCharacter::MoveRight);
	
	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	InputComponent->BindAxis("TurnRate", this, &AUTCharacter::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &AUTCharacter::LookUpAtRate);
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
}
void AUTCharacter::FiringInfoUpdated()
{
	if (Weapon != NULL && !FlashLocation.IsZero())  // and in first person?
	{
		Weapon->PlayImpactEffects(FlashLocation);
	}
	// TODO: weapon attachment
}

void AUTCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// find out which way is forward
		const FRotator Rotation = GetControlRotation();
		FRotator YawRotation(0, Rotation.Yaw, 0);

		// Get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void AUTCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// find out which way is right
		const FRotator Rotation = GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// Get right vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

void AUTCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AUTCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
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
	if (InvToRemove != NULL)
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
				// TODO: implement
				//Controller->SwitchToBestWeapon();
			}
		}
		InvToRemove->eventRemoved();
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
	if (Role <= ROLE_SimulatedProxy)
	{
		UE_LOG(UT, Warning, TEXT("Illegal SwitchWeapon() call on remote client"));
	}
	else if (NewWeapon == NULL || NewWeapon->Instigator != this || !IsInInventory(NewWeapon))
	{
		UE_LOG(UT, Warning, TEXT("Weapon %s is not owned by self"), *NewWeapon->GetName());
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

void AUTCharacter::LocalSwitchWeapon(AUTWeapon* NewWeapon)
{
	if (Weapon == NULL)
	{
		Weapon = NewWeapon;
		Weapon->BringUp();
	}
	else if (Weapon->PutDown())
	{
		PendingWeapon = NewWeapon;
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
		PendingWeapon = NULL;
		Weapon->BringUp();
	}
	else
	{
		// restore current weapon since pending became invalid
		Weapon->BringUp();
	}
}

void AUTCharacter::CheckAutoWeaponSwitch(AUTWeapon* TestWeapon)
{
	if (IsLocallyControlled())
	{
		// TODO: some ratings comparison or something
		SwitchWeapon(TestWeapon);
	}
	else
	{
		UE_LOG(UT, Warning, TEXT("Illegal CheckAutoWeaponSwitch(); %s not locally controlled"), *GetName());
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
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashLocation, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireMode, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, LastTakeHitInfo, COND_SimulatedOnly);
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

void AUTCharacter::UnPossessed()
{
	UE_LOG(UT,Warning,TEXT("UNPOSSESS"));
	Super::UnPossessed();
}
