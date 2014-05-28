// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHud.h"
#include "UTPlayerState.h"
#include "UTPlayerController.h"
#include "ActiveSound.h"
#include "AudioDevice.h"

AUTPlayerController::AUTPlayerController(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	bAutoWeaponSwitch = true;

	MaxDodgeClickTime = 0.25f;
	LastTapLeftTime = -10.f;
	LastTapRightTime = -10.f;
	LastTapForwardTime = -10.f;
	LastTapBackTime = -10.f;
}

void AUTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAxis("MoveForward", this, &AUTPlayerController::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AUTPlayerController::MoveRight);
	InputComponent->BindAction("Jump", IE_Pressed, this, &AUTPlayerController::Jump);

	InputComponent->BindAction("TapLeft", IE_Pressed, this, &AUTPlayerController::OnTapLeft);
	InputComponent->BindAction("TapRight", IE_Pressed, this, &AUTPlayerController::OnTapRight);
	InputComponent->BindAction("TapForward", IE_Pressed, this, &AUTPlayerController::OnTapForward);
	InputComponent->BindAction("TapBack", IE_Pressed, this, &AUTPlayerController::OnTapBack);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APlayerController::AddYawInput);
	InputComponent->BindAxis("TurnRate", this, &AUTPlayerController::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APlayerController::AddPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &AUTPlayerController::LookUpAtRate);

	InputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AUTPlayerController::PrevWeapon);
	InputComponent->BindAction("NextWeapon", IE_Released, this, &AUTPlayerController::NextWeapon);

	InputComponent->BindAction("StartFire", IE_Pressed, this, &AUTPlayerController::OnFire);
	InputComponent->BindAction("StopFire", IE_Released, this, &AUTPlayerController::OnStopFire);
	InputComponent->BindAction("StartAltFire", IE_Pressed, this, &AUTPlayerController::OnAltFire);
	InputComponent->BindAction("StopAltFire", IE_Released, this, &AUTPlayerController::OnStopAltFire);
	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AUTPlayerController::TouchStarted);
}

/* Cache a copy of the PlayerState cast'd to AUTPlayerState for easy reference.  Do it both here and when the replicated copy of APlayerState arrives in OnRep_PlayerState */
void AUTPlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
}

void AUTPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
}

void AUTPlayerController::SetPawn(APawn* InPawn)
{
	Super::SetPawn(InPawn);
	UTCharacter = Cast<AUTCharacter>(InPawn);
}

void AUTPlayerController::SwitchToBestWeapon()
{
	if (UTCharacter != NULL)
	{
		AUTWeapon* BestWeapon = NULL;
		for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
			if (Weap != NULL)
			{
				if (BestWeapon == NULL || Weap->Group > BestWeapon->Group)
				{
					BestWeapon = Weap;
				}
			}
		}
		UTCharacter->SwitchWeapon(BestWeapon);
	}
}
void AUTPlayerController::PrevWeapon()
{
	SwitchWeaponInSequence(true);
}
void AUTPlayerController::NextWeapon()
{
	SwitchWeaponInSequence(false);
}
void AUTPlayerController::SwitchWeaponInSequence(bool bPrev)
{
	if (UTCharacter != NULL)
	{
		if (UTCharacter->GetWeapon() == NULL)
		{
			SwitchToBestWeapon();
		}
		else
		{
			AUTWeapon* Best = NULL;
			AUTWeapon* WraparoundChoice = NULL;
			int32 CurrentGroup = (UTCharacter->GetPendingWeapon() != NULL) ? UTCharacter->GetPendingWeapon()->Group : UTCharacter->GetWeapon()->Group;
			int32 CurrentSlot = (UTCharacter->GetPendingWeapon() != NULL) ? UTCharacter->GetPendingWeapon()->GroupSlot : UTCharacter->GetWeapon()->GroupSlot;
			int32 BestGroup = 0, BestSlot = 0;
			int32 WrapGroup = 0, WrapSlot = 0;
			for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
			{
				AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
				if (Weap != NULL && Weap != UTCharacter->GetWeapon())
				{
					if (bPrev)
					{
						if ( (Weap->Group < CurrentGroup || (Weap->Group == CurrentGroup && Weap->GroupSlot < CurrentSlot)) &&
							(Best == NULL || Weap->Group > BestGroup || Weap->GroupSlot > BestSlot) )
						{
							Best = Weap;
						}
						if (WraparoundChoice == NULL || Weap->Group > WrapGroup || (Weap->Group == WrapGroup && Weap->GroupSlot > WrapSlot))
						{
							WraparoundChoice = Weap;
						}
					}
					else
					{
						if ( (Weap->Group > CurrentGroup || (Weap->Group == CurrentGroup && Weap->GroupSlot > CurrentSlot)) &&
							(Best == NULL || Weap->Group < BestGroup || Weap->GroupSlot < BestSlot) )
						{
							Best = Weap;
						}
						if (WraparoundChoice == NULL || Weap->Group < WrapGroup || (Weap->Group == WrapGroup && Weap->GroupSlot < WrapSlot))
						{
							WraparoundChoice = Weap;
						}
					}
				}
			}
			if (Best == NULL)
			{
				Best = WraparoundChoice;
			}
			UTCharacter->SwitchWeapon(Best);
		}
	}
}
void AUTPlayerController::CheckAutoWeaponSwitch(AUTWeapon* TestWeapon)
{
	if (UTCharacter != NULL)
	{
		AUTWeapon* CurWeapon = UTCharacter->GetPendingWeapon();
		if (CurWeapon == NULL)
		{
			CurWeapon = UTCharacter->GetWeapon();
		}
		if (CurWeapon == NULL || (bAutoWeaponSwitch && TestWeapon->Group > CurWeapon->Group))
		{
			UTCharacter->SwitchWeapon(TestWeapon);
		}
	}
}
void AUTPlayerController::SwitchWeapon(int32 Group)
{
	if (UTCharacter != NULL)
	{
		// if current weapon isn't in the specified group, pick lowest GroupSlot in that group
		// if it is, then pick next highest slot, or wrap around to lowest if no higher slot
		AUTWeapon* CurrWeapon = (UTCharacter->GetPendingWeapon() != NULL) ? UTCharacter->GetPendingWeapon() : UTCharacter->GetWeapon();
		AUTWeapon* LowestSlotWeapon = NULL;
		AUTWeapon* NextSlotWeapon = NULL;
		for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
			if (Weap != NULL && Weap != UTCharacter->GetWeapon())
			{
				if (Weap->Group == Group)
				{
					if (LowestSlotWeapon == NULL || LowestSlotWeapon->GroupSlot > Weap->GroupSlot)
					{
						LowestSlotWeapon = Weap;
					}
					if (CurrWeapon != NULL && CurrWeapon->Group == Group && Weap->GroupSlot > CurrWeapon->GroupSlot && (NextSlotWeapon == NULL || NextSlotWeapon->GroupSlot > Weap->GroupSlot))
					{
						NextSlotWeapon = Weap;
					}
				}
			}
		}
		if (NextSlotWeapon != NULL)
		{
			UTCharacter->SwitchWeapon(NextSlotWeapon);
		}
		else if (LowestSlotWeapon != NULL)
		{
			UTCharacter->SwitchWeapon(LowestSlotWeapon);
		}
	}
}

void AUTPlayerController::OnFire()
{
	if (UTCharacter != NULL)
	{
		UTCharacter->StartFire(0);
	}
	else
	{
		ServerRestartPlayer();
	}

}
void AUTPlayerController::OnStopFire()
{
	if (UTCharacter != NULL)
	{
		UTCharacter->StopFire(0);
	}
}
void AUTPlayerController::OnAltFire()
{
	if (UTCharacter != NULL)
	{
		UTCharacter->StartFire(1);
	}
}
void AUTPlayerController::OnStopAltFire()
{
	if (UTCharacter != NULL)
	{
		UTCharacter->StopFire(1);
	}
}
void AUTPlayerController::MoveForward(float Value)
{
	if (Value != 0.0f && GetPawn() != NULL)
	{
		// find out which way is forward
		const FRotator Rotation = GetControlRotation();
		FRotator YawRotation(0, Rotation.Yaw, 0);

		// Get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// add movement in that direction
		GetPawn()->AddMovementInput(Direction, Value);
	}
}
void AUTPlayerController::MoveRight(float Value)
{
	if (Value != 0.0f && GetPawn() != NULL)
	{
		// find out which way is right
		const FRotator Rotation = GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// Get right vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement in that direction
		GetPawn()->AddMovementInput(Direction, Value);
	}
}
void AUTPlayerController::TurnAtRate(float Rate)
{
	if (GetPawn() != NULL)
	{
		// calculate delta for this frame from the rate information
		AddYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
	}
}
void AUTPlayerController::LookUpAtRate(float Rate)
{
	if (GetPawn() != NULL)
	{
		// calculate delta for this frame from the rate information
		AddPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
	}
}
void AUTPlayerController::Jump()
{
	if (GetCharacter() != NULL)
	{
		GetCharacter()->bPressedJump = true;
	}
}

void AUTPlayerController::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// only fire for first finger down
	if (FingerIndex == 0)
	{
		OnFire();
	}
}

void AUTPlayerController::HearSound(USoundBase* InSoundCue, AActor* SoundPlayer, const FVector& SoundLocation, bool bStopWhenOwnerDestroyed)
{
	bool bIsOccluded = false;
	if (SoundPlayer == this || (GetViewTarget() != NULL && InSoundCue->IsAudible(SoundLocation, GetViewTarget()->GetActorLocation(), (SoundPlayer != NULL) ? SoundPlayer : this, bIsOccluded, true)))
	{
		// we don't want to replicate the location if it's the same as Actor location (so the sound gets played attached to the Actor), but we must if the source Actor isn't relevant
		UNetConnection* Conn = Cast<UNetConnection>(Player);
		FVector RepLoc = (SoundPlayer != NULL && SoundPlayer->GetActorLocation() == SoundLocation && Conn->ActorChannels.Contains(SoundPlayer)) ? FVector::ZeroVector : SoundLocation;
		ClientHearSound(InSoundCue, SoundPlayer, RepLoc, bStopWhenOwnerDestroyed, bIsOccluded);
	}
}

void AUTPlayerController::ClientHearSound_Implementation(USoundBase* TheSound, AActor* SoundPlayer, FVector SoundLocation, bool bStopWhenOwnerDestroyed, bool bIsOccluded)
{
	if (SoundPlayer == this || SoundPlayer == GetViewTarget())
	{
		// no attenuation/spatialization, full volume
		FActiveSound NewActiveSound;
		NewActiveSound.World = GetWorld();
		NewActiveSound.Sound = TheSound;

		NewActiveSound.VolumeMultiplier = 1.0f;
		NewActiveSound.PitchMultiplier = 1.0f;

		NewActiveSound.RequestedStartTime = 0.0f;

		NewActiveSound.bLocationDefined = false;
		NewActiveSound.bIsUISound = false;
		NewActiveSound.bHasAttenuationSettings = false;
		NewActiveSound.bAllowSpatialization = false;

		// TODO - Audio Threading. This call would be a task call to dispatch to the audio thread
		GEngine->GetAudioDevice()->AddNewActiveSound(NewActiveSound);
	}
	else if (!SoundLocation.IsZero())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), TheSound, SoundLocation, bIsOccluded ? 0.5f : 1.0f);
	}
	else if (SoundPlayer != NULL)
	{
		UGameplayStatics::PlaySoundAttached(TheSound, SoundPlayer->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, bStopWhenOwnerDestroyed, bIsOccluded ? 0.5f : 1.0f);
	}
}

bool AUTPlayerController::CheckDodge(float LastTapTime, float DodgeDirX, float DodgeDirY, float DodgeCrossX, float DodgeCrossY)
{
	if ( UTCharacter && (GetWorld()->GetTimeSeconds() - LastTapTime < MaxDodgeClickTime) )
	{
		FRotator TurnRot(0.f, UTCharacter->GetActorRotation().Yaw, 0.f);
		FRotationMatrix TurnRotMatrix = FRotationMatrix(TurnRot);
		FVector X = TurnRotMatrix.GetScaledAxis(EAxis::X);
		FVector Y = TurnRotMatrix.GetScaledAxis(EAxis::Y);
		UTCharacter->Dodge(DodgeDirX*X + DodgeDirY*Y, DodgeCrossX*X + DodgeCrossY*Y);
	}
	return false;
}

void AUTPlayerController::OnTapLeft()
{
	LastTapLeftTime = (!UTCharacter || CheckDodge(LastTapLeftTime, 0.f, -1.f, 1.f, 0.f)) ? -10.f : GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapRight()
{
	LastTapRightTime = (!UTCharacter || CheckDodge(LastTapRightTime, 0.f, 1.f, 1.f, 0.f)) ? -10.f : GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapForward()
{
	LastTapForwardTime = (!UTCharacter || CheckDodge(LastTapForwardTime, 1.f, 0.f, 0.f, 1.f)) ? -10.f : GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapBack()
{
	LastTapBackTime = (!UTCharacter || CheckDodge(LastTapBackTime, -1.f, 0.f, 0.f, 1.f)) ? -10.f : GetWorld()->GetTimeSeconds();
}


