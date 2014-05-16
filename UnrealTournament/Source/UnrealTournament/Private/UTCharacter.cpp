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
	FirstPersonMesh->RelativeLocation = FVector(0.f, 0.f, -150.f);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;
}

void AUTCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// TODO: should be done by game type
	AddInventory(GetWorld()->SpawnActor<AUTWeapon>(DefaultWeapon, FVector(0.0f), FRotator(0, 0, 0)), true);
}

//////////////////////////////////////////////////////////////////////////
// Input

void AUTCharacter::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	// set up gameplay key bindings
	check(InputComponent);

	InputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);

	InputComponent->BindAction("StartFire", IE_Pressed, this, &AUTCharacter::OnFire);
	InputComponent->BindAction("StopFire", IE_Released, this, &AUTCharacter::OnStopFire);
	InputComponent->BindAction("StartAltFire", IE_Pressed, this, &AUTCharacter::OnAltFire);
	InputComponent->BindAction("StopAltFire", IE_Released, this, &AUTCharacter::OnStopAltFire);
	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AUTCharacter::TouchStarted);

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

void AUTCharacter::OnFire()
{
	StartFire(0);
}
void AUTCharacter::OnStopFire()
{
	StopFire(0);
}
void AUTCharacter::OnAltFire()
{
	StartFire(1);
}
void AUTCharacter::OnStopAltFire()
{
	StopFire(1);
}

void AUTCharacter::StartFire(uint8 FireModeNum)
{
	if (!IsLocallyControlled())
	{
		UE_LOG(UT, Warning, TEXT("StartFire() can only be called on the owning client"));
	}
	else if (Weapon != NULL)
	{
		if (PendingFire.Num() < FireModeNum + 1)
		{
			PendingFire.SetNumZeroed(FireModeNum + 1);
		}
		if (PendingFire[FireModeNum] == 0)
		{
			PendingFire[FireModeNum] = 1;
			Weapon->StartFire(FireModeNum);
		}
	}
}

void AUTCharacter::StopFire(uint8 FireModeNum)
{
	if (!IsLocallyControlled())
	{
		UE_LOG(UT, Warning, TEXT("StopFire() can only be called on the owning client"));
	}
	else if (Weapon != NULL && IsPendingFire(FireModeNum))
	{
		PendingFire[FireModeNum] = 0;
		Weapon->StopFire(FireModeNum);
	}
}

void AUTCharacter::SetFlashLocation(const FVector& InFlashLoc, uint8 InFireMode)
{
	FlashLocation = InFlashLoc;
	// we reserve the zero vector to stop firing, so make sure we aren't setting a value that would replicate that way
	if (FlashLocation.SizeSquared() < 1.0f)
	{
		FlashLocation.Z += 1.1f;
	}
	FireMode = InFireMode;
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
}
void AUTCharacter::ClearFiringInfo()
{
	FlashLocation = FVector::ZeroVector;
	FlashCount = 0;
}
void AUTCharacter::FiringInfoUpdated()
{
	if (FlashLocation.IsZero() && FlashCount == 0)
	{
		if (Weapon != NULL) // and in first person?
		{
			Weapon->StopFiringEffects();
		}
	}
	else
	{
		if (Weapon != NULL)  // and in first person?
		{
			Weapon->PlayFiringEffects();
		}
	}
}

void AUTCharacter::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// only fire for first finger down
	if (FingerIndex == 0)
	{
		OnFire();
	}
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

void AUTCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	DOREPLIFETIME_CONDITION(AUTCharacter, InventoryList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, FlashLocation, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(AUTCharacter, FireMode, COND_SkipOwner);
}
