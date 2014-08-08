// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTHUD.h"
#include "UTLocalPlayer.h"
#include "UTPlayerState.h"
#include "UTPlayerController.h"
#include "UTCharacterMovement.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "UTPickup.h"
#include "UTPickupInventory.h"
#include "UTPickupWeapon.h"
#include "UTAnnouncer.h"
#include "UTHUDWidgetMessage.h"
#include "UTPlayerInput.h"
#include "UTPlayerCameraManager.h"
#include "UTCheatManager.h"
#include "UTCTFGameState.h"
#include "UTChatMessage.h"

DEFINE_LOG_CATEGORY_STATIC(LogUTPlayerController, Log, All);

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

	PlayerCameraManagerClass = AUTPlayerCameraManager::StaticClass();
	CheatClass = UUTCheatManager::StaticClass();

	WeaponBobGlobalScaling = 1.f;
	EyeOffsetGlobalScaling = 1.f;

	ConfigDefaultFOV = 90.0f;
	FFAPlayerColor = FLinearColor(0.020845f, 0.335f, 0.0f, 1.0f);
}

void AUTPlayerController::SetEyeOffsetScaling(float NewScaling)
{
	EyeOffsetGlobalScaling = NewScaling;
}

void AUTPlayerController::SetWeaponBobScaling(float NewScaling)
{
	WeaponBobGlobalScaling = NewScaling;
}

FVector AUTPlayerController::GetFocalLocation() const
{
	if (GetPawnOrSpectator())
	{
		return GetPawnOrSpectator()->GetPawnViewLocation();
	}
	else
	{
		return GetSpawnLocation();
	}
}

void AUTPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindAxis("MoveForward", this, &AUTPlayerController::MoveForward);
	InputComponent->BindAxis("MoveRight", this, &AUTPlayerController::MoveRight);
	InputComponent->BindAxis("MoveUp", this, &AUTPlayerController::MoveUp);
	InputComponent->BindAction("Jump", IE_Pressed, this, &AUTPlayerController::Jump);
	InputComponent->BindAction("Crouch", IE_Pressed, this, &AUTPlayerController::Crouch);
	InputComponent->BindAction("Crouch", IE_Released, this, &AUTPlayerController::UnCrouch);
	InputComponent->BindAction("ToggleCrouch", IE_Pressed, this, &AUTPlayerController::ToggleCrouch);

	InputComponent->BindAction("TapLeft", IE_Pressed, this, &AUTPlayerController::OnTapLeft);
	InputComponent->BindAction("TapRight", IE_Pressed, this, &AUTPlayerController::OnTapRight);
	InputComponent->BindAction("TapForward", IE_Pressed, this, &AUTPlayerController::OnTapForward);
	InputComponent->BindAction("TapBack", IE_Pressed, this, &AUTPlayerController::OnTapBack);
	InputComponent->BindAction("SingleTapDodge", IE_Pressed, this, &AUTPlayerController::OnSingleTapDodge);
	InputComponent->BindAction("HoldDodge", IE_Pressed, this, &AUTPlayerController::HoldDodge);
	InputComponent->BindAction("HoldDodge", IE_Released, this, &AUTPlayerController::ReleaseDodge);
	InputComponent->BindAction("DodgeRoll", IE_Released, this, &AUTPlayerController::OnDodgeRoll);
	InputComponent->BindAction("JumpDodgeRoll", IE_Released, this, &AUTPlayerController::OnJumpDodgeRoll);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APlayerController::AddYawInput);
	InputComponent->BindAxis("TurnRate", this, &AUTPlayerController::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APlayerController::AddPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &AUTPlayerController::LookUpAtRate);

	InputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AUTPlayerController::PrevWeapon);
	InputComponent->BindAction("NextWeapon", IE_Released, this, &AUTPlayerController::NextWeapon);
	InputComponent->BindAction("ToggleTranslocator", IE_Pressed, this, &AUTPlayerController::ToggleTranslocator);

	InputComponent->BindAction("StartFire", IE_Pressed, this, &AUTPlayerController::OnFire);
	InputComponent->BindAction("StopFire", IE_Released, this, &AUTPlayerController::OnStopFire);
	InputComponent->BindAction("StartAltFire", IE_Pressed, this, &AUTPlayerController::OnAltFire);
	InputComponent->BindAction("StopAltFire", IE_Released, this, &AUTPlayerController::OnStopAltFire);
	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AUTPlayerController::TouchStarted);

	InputComponent->BindAction("ShowScores", IE_Pressed, this, &AUTPlayerController::OnShowScores);
	InputComponent->BindAction("ShowScores", IE_Released, this, &AUTPlayerController::OnHideScores);

	InputComponent->BindAction("ShowMenu", IE_Released, this, &AUTPlayerController::ShowMenu);
}

void AUTPlayerController::ProcessPlayerInput(const float DeltaTime, const bool bGamePaused)
{
	Super::ProcessPlayerInput(DeltaTime, bGamePaused);

	if (bRequestedDodge)
	{
		PerformSingleTapDodge();
	}
	bRequestedDodge = false;
	MovementForwardAxis = 0.f;
	MovementStrafeAxis = 0.f;
}

void AUTPlayerController::InitInputSystem()
{
	if (PlayerInput == NULL)
	{
		PlayerInput = ConstructObject<UUTPlayerInput>(UUTPlayerInput::StaticClass(), this);
	}

	Super::InitInputSystem();

	if (RewardAnnouncerPath.ClassName.Len() > 0)
	{
		TSubclassOf<UUTAnnouncer> RewardAnnouncerClass = LoadClass<UUTAnnouncer>(NULL, *RewardAnnouncerPath.ClassName, NULL, 0, NULL);
		if (RewardAnnouncerClass != NULL && RewardAnnouncerClass.GetDefaultObject()->IsRewardAnnouncer())
		{
			RewardAnnouncer = NewObject<UUTAnnouncer>(this, RewardAnnouncerClass);
		}
	}
	if (StatusAnnouncerPath.ClassName.Len() > 0)
	{
		TSubclassOf<UUTAnnouncer> StatusAnnouncerClass = LoadClass<UUTAnnouncer>(NULL, *StatusAnnouncerPath.ClassName, NULL, 0, NULL);
		if (StatusAnnouncerClass != NULL && StatusAnnouncerClass.GetDefaultObject()->IsStatusAnnouncer())
		{
			StatusAnnouncer = NewObject<UUTAnnouncer>(this, StatusAnnouncerClass);
		}
	}
}

/* Cache a copy of the PlayerState cast'd to AUTPlayerState for easy reference.  Do it both here and when the replicated copy of APlayerState arrives in OnRep_PlayerState */
void AUTPlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
	
	// need this until Controller::InitPlayerState() is updated
	if (PlayerState && PlayerState->PlayerName.IsEmpty())
	{
		UWorld* const World = GetWorld();
		if (World)
		{
			AGameMode* const GameMode = World->GetAuthGameMode();
			if (GameMode)
			{
				// don't call SetPlayerName() as that will broadcast entry messages but the GameMode hasn't had a chance
				// to potentially apply a player/bot name yet
				PlayerState->PlayerName = GameMode->DefaultPlayerName;
			}
		}
	}
}

void AUTPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
}

void AUTPlayerController::SetPawn(APawn* InPawn)
{
	if (InPawn == NULL)
	{
		// Attempt to move the PC to the current camera location if no pawn was specified
		const FVector NewLocation = (PlayerCameraManager != NULL) ? PlayerCameraManager->GetCameraLocation() : GetSpawnLocation();
		SetSpawnLocation(NewLocation);
	}

	AController::SetPawn(InPawn);

	UTCharacter = Cast<AUTCharacter>(InPawn);

	// apply FOV angle if dead/spectating
	if (GetPawn() == NULL && IsLocalPlayerController() && PlayerCameraManager != NULL)
	{
		FOV(ConfigDefaultFOV);
	}
}

void AUTPlayerController::SpawnPlayerCameraManager()
{
	Super::SpawnPlayerCameraManager();
	// init configured FOV angle
	FOV(ConfigDefaultFOV);
}

void AUTPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	Super::ClientRestart_Implementation(NewPawn);

	// make sure we don't have leftover zoom
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->UnlockFOV();
	}
}

void AUTPlayerController::FOV(float NewFOV)
{
	if (NewFOV != ConfigDefaultFOV)
	{
		ConfigDefaultFOV = FMath::Clamp<float>(NewFOV, FOV_CONFIG_MIN, FOV_CONFIG_MAX);
		if (GetPawn() != NULL)
		{
			Suicide();
		}
		SaveConfig();
	}
}

bool AUTPlayerController::InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	// unfortunately have to go roundabout because this is the only InputKey() that's virtual
	UUTPlayerInput* Input = Cast<UUTPlayerInput>(PlayerInput);
	if (Input != NULL)
	{
		Input->ExecuteCustomBind(Key, EventType);
		// ...unsure if we should eat the input on success
	}
	return Super::InputKey(Key, EventType, AmountDepressed, bGamepad);
}

void AUTPlayerController::SwitchToBestWeapon()
{
	if (UTCharacter != NULL && IsLocalPlayerController())
	{
		AUTWeapon* BestWeapon = NULL;
		float BestPriority = 0.0f;
		for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
			if (Weap != NULL && Weap->HasAnyAmmo())
			{
				float TestPriority = Weap->GetAutoSwitchPriority();
				if (TestPriority > BestPriority)
				{
					BestWeapon = Weap;
					BestPriority = TestPriority;
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
void AUTPlayerController::ToggleTranslocator()
{
	if (UTCharacter != NULL && IsLocalPlayerController())
	{
		if (UTCharacter->GetWeapon()->Group == 0)
		{
			SwitchWeapon(PreviousWeaponGroup);
		}
		else
		{
			PreviousWeaponGroup = UTCharacter->GetWeapon()->Group;
			SwitchWeapon(0);
		}
	}

}
void AUTPlayerController::SwitchWeaponInSequence(bool bPrev)
{
	if (UTCharacter != NULL && IsLocalPlayerController())
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
			for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
			{
				AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
				if (Weap != NULL && Weap != UTCharacter->GetWeapon() && Weap->HasAnyAmmo())
				{
					if (bPrev)
					{
						if ( (Weap->Group < CurrentGroup || (Weap->Group == CurrentGroup && Weap->GroupSlot < CurrentSlot)) &&
							(Best == NULL || Weap->Group > Best->Group || Weap->GroupSlot > Best->GroupSlot) )
						{
							Best = Weap;
						}
						if (WraparoundChoice == NULL || Weap->Group > WraparoundChoice->Group || (Weap->Group == WraparoundChoice->Group && Weap->GroupSlot > WraparoundChoice->GroupSlot))
						{
							WraparoundChoice = Weap;
						}
					}
					else
					{
						if ( (Weap->Group > CurrentGroup || (Weap->Group == CurrentGroup && Weap->GroupSlot > CurrentSlot)) &&
							(Best == NULL || Weap->Group < Best->Group || Weap->GroupSlot < Best->GroupSlot) )
						{
							Best = Weap;
						}
						if (WraparoundChoice == NULL || Weap->Group < WraparoundChoice->Group || (Weap->Group == WraparoundChoice->Group && Weap->GroupSlot < WraparoundChoice->GroupSlot))
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
	if (UTCharacter != NULL && IsLocalPlayerController())
	{
		AUTWeapon* CurWeapon = UTCharacter->GetPendingWeapon();
		if (CurWeapon == NULL)
		{
			CurWeapon = UTCharacter->GetWeapon();
		}
		if (CurWeapon == NULL || (bAutoWeaponSwitch && !CurWeapon->IsFiring() && TestWeapon->GetAutoSwitchPriority() > CurWeapon->GetAutoSwitchPriority()))
		{
			UTCharacter->SwitchWeapon(TestWeapon);
		}
	}
}
void AUTPlayerController::SwitchWeapon(int32 Group)
{
	if (UTCharacter != NULL && IsLocalPlayerController())
	{
		// if current weapon isn't in the specified group, pick lowest GroupSlot in that group
		// if it is, then pick next highest slot, or wrap around to lowest if no higher slot
		AUTWeapon* CurrWeapon = (UTCharacter->GetPendingWeapon() != NULL) ? UTCharacter->GetPendingWeapon() : UTCharacter->GetWeapon();
		AUTWeapon* LowestSlotWeapon = NULL;
		AUTWeapon* NextSlotWeapon = NULL;
		for (AUTInventory* Inv = UTCharacter->GetInventory(); Inv != NULL; Inv = Inv->GetNext())
		{
			AUTWeapon* Weap = Cast<AUTWeapon>(Inv);
			if (Weap != NULL && Weap != UTCharacter->GetWeapon() && Weap->HasAnyAmmo())
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
		if (StateName == NAME_Playing)
		{
			UTCharacter->StartFire(0);
		}
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
		if (StateName == NAME_Playing)
		{
			UTCharacter->StartFire(1);
		}
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
	if (Value != 0.0f && UTCharacter != NULL)
	{
		MovementForwardAxis = Value;
		UTCharacter->MoveForward(Value);
	}
}

void AUTPlayerController::MoveRight(float Value)
{
	if (Value != 0.0f && UTCharacter != NULL)
	{
		MovementStrafeAxis = Value;
		UTCharacter->MoveRight(Value);
	}
}

void AUTPlayerController::MoveUp(float Value)
{
	if (Value != 0.0f && UTCharacter != NULL)
	{
		UTCharacter->MoveUp(Value);
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

void AUTPlayerController::Crouch()
{
	if (GetCharacter() != NULL)
	{
		GetCharacter()->Crouch(false);
	}
}

void AUTPlayerController::UnCrouch()
{
	if (GetCharacter() != NULL)
	{
		GetCharacter()->UnCrouch(false);
	}
}

void AUTPlayerController::ToggleCrouch()
{
	if (GetCharacter() != nullptr)
	{
		GetCharacter()->bIsCrouched ? UnCrouch() : Crouch();
	}
}

void AUTPlayerController::DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos)
{
	UFont* RenderFont = GEngine->GetSmallFont();
	if (GetPawn() == NULL)
	{
		if (PlayerState == NULL)
		{
			Canvas->DrawText(RenderFont, TEXT("NO PlayerState"), 4.0f, YPos);
		}
		else
		{
			PlayerState->DisplayDebug(Canvas, DebugDisplay, YL, YPos);
		}
		YPos += YL;

		return;
	}

	Canvas->SetDrawColor(255, 0, 0);
	Canvas->DrawText(RenderFont, FString::Printf(TEXT("CONTROLLER %s Pawn %s"), *GetName(), *GetPawn()->GetName()), 4.0f, YPos);
	YPos += YL;

	Canvas->SetDrawColor(255, 255, 0);
	Canvas->DrawText(RenderFont, FString::Printf(TEXT("STATE %s"), *GetStateName().ToString()), 4.0f, YPos);
	YPos += YL;

	Super::DisplayDebug(Canvas, DebugDisplay, YL, YPos);
}


void AUTPlayerController::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
{
	// only fire for first finger down
	if (FingerIndex == 0)
	{
		OnFire();
	}
}

void AUTPlayerController::HearSound(USoundBase* InSoundCue, AActor* SoundPlayer, const FVector& SoundLocation, bool bStopWhenOwnerDestroyed, bool bAmplifyVolume)
{
	bool bIsOccluded = false;
	if (SoundPlayer == this || (GetViewTarget() != NULL && InSoundCue->IsAudible(SoundLocation, GetViewTarget()->GetActorLocation(), (SoundPlayer != NULL) ? SoundPlayer : this, bIsOccluded, true)))
	{
		// we don't want to replicate the location if it's the same as Actor location (so the sound gets played attached to the Actor), but we must if the source Actor isn't relevant
		UNetConnection* Conn = Cast<UNetConnection>(Player);
		FVector RepLoc = (SoundPlayer != NULL && SoundPlayer->GetActorLocation() == SoundLocation && (Conn == NULL || Conn->ActorChannels.Contains(SoundPlayer))) ? FVector::ZeroVector : SoundLocation;
		ClientHearSound(InSoundCue, SoundPlayer, RepLoc, bStopWhenOwnerDestroyed, bIsOccluded, bAmplifyVolume);
	}
}

void AUTPlayerController::ClientHearSound_Implementation(USoundBase* TheSound, AActor* SoundPlayer, FVector SoundLocation, bool bStopWhenOwnerDestroyed, bool bIsOccluded, bool bAmplifyVolume)
{
	if (TheSound != NULL && (SoundPlayer != NULL || !SoundLocation.IsZero()))
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
			if (GEngine->GetAudioDevice() != NULL)
			{
				GEngine->GetAudioDevice()->AddNewActiveSound(NewActiveSound);
			}
		}
		else
		{
			USoundAttenuation* AttenuationOverride = NULL;
			if (bAmplifyVolume)
			{
				// the UGameplayStatics functions copy the FAttenuationSettings by value so no need to create more than one, just reuse
				static USoundAttenuation* OverrideObj = [](){ USoundAttenuation* Result = NewObject<USoundAttenuation>(); Result->AddToRoot(); return Result; }();

				AttenuationOverride = OverrideObj;
				const FAttenuationSettings* DefaultAttenuation = TheSound->GetAttenuationSettingsToApply();
				if (DefaultAttenuation != NULL)
				{
					AttenuationOverride->Attenuation = *DefaultAttenuation;
				}
				// set minimum volume
				// we're assuming that the radius was already checked via HearSound() and thus this won't cause hearing the audio level-wide
				AttenuationOverride->Attenuation.dBAttenuationAtMax = 30.0f;
				// move sound closer
				AActor* ViewTarget = GetViewTarget();
				if (ViewTarget != NULL)
				{
					if (SoundLocation.IsZero())
					{
						SoundLocation = SoundPlayer->GetActorLocation();
					}
					FVector SoundOffset = GetViewTarget()->GetActorLocation() - SoundLocation;
					SoundLocation = SoundLocation + SoundOffset * FMath::Min<float>(SoundOffset.Size() * 0.25f, 2000.0f);
				}
			}
			float VolumeMultiplier = bIsOccluded ? 0.5f : 1.0f;
			if (!SoundLocation.IsZero() && (SoundPlayer == NULL || SoundLocation != SoundPlayer->GetActorLocation()))
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), TheSound, SoundLocation, VolumeMultiplier, 1.0f, 0.0f, AttenuationOverride);
			}
			else if (SoundPlayer != NULL)
			{
				UGameplayStatics::PlaySoundAttached(TheSound, SoundPlayer->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, bStopWhenOwnerDestroyed, VolumeMultiplier, 1.0f, 0.0f, AttenuationOverride);
			}
		}
	}
}

bool AUTPlayerController::CheckDodge(float LastTapTime, bool bForward, bool bBack, bool bLeft, bool bRight)
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement && (bIsHoldingDodge || (GetWorld()->GetTimeSeconds() - LastTapTime < MaxDodgeClickTime)))
	{
		MyCharMovement->bPressedDodgeForward = bForward;
		MyCharMovement->bPressedDodgeBack = bBack;
		MyCharMovement->bPressedDodgeLeft = bLeft;
		MyCharMovement->bPressedDodgeRight = bRight;
	}
	return false;
}

void AUTPlayerController::OnJumpDodgeRoll()
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement && !MyCharMovement->bAllowDodgeMultijumps)
	{
		OnDodgeRoll();
	}
}

void AUTPlayerController::OnDodgeRoll()
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement && MyCharMovement->bIsDodging && (MyCharMovement->Velocity.Z < MyCharMovement->DodgeRollEarliestZ))
	{
		MyCharMovement->DodgeRollTapTime = MyCharMovement->GetCurrentMovementTime();
		MyCharMovement->bWillDodgeRoll = true;
	}
}

void AUTPlayerController::OnSingleTapDodge()
{
	bRequestedDodge = true;
}

void AUTPlayerController::PerformSingleTapDodge()
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement)
	{
		// base dodge direction on currently pressed axis movement.  
		// If two directions pressed, dodge to the side
		MyCharMovement->bPressedDodgeForward = false;
		MyCharMovement->bPressedDodgeBack = false;
		MyCharMovement->bPressedDodgeLeft = false;
		MyCharMovement->bPressedDodgeRight = false;

		// @TODO FIXMESTEVE need to check pressed move dir, otherwise fast turn will cause wrong dir dodge
		if (MovementStrafeAxis > 0.5f)
		{
			MyCharMovement->bPressedDodgeRight = true;
			UE_LOG(LogUTPlayerController, Verbose, TEXT("SingleTapDodge Right"));
		}
		else if (MovementStrafeAxis < -0.5f)
		{
			MyCharMovement->bPressedDodgeLeft = true;
			UE_LOG(LogUTPlayerController, Verbose, TEXT("SingleTapDodge Left"));
		}
		else if ( MovementForwardAxis >= 0.f)
		{
			MyCharMovement->bPressedDodgeForward = true;
			UE_LOG(LogUTPlayerController, Verbose, TEXT("SingleTapDodge Forward"));
		}
		else
		{
			MyCharMovement->bPressedDodgeBack = true;
			UE_LOG(LogUTPlayerController, Verbose, TEXT("SingleTapDodge Back"));
		}
	}
}

void AUTPlayerController::HoldDodge()
{
	bIsHoldingDodge = true;
}

void AUTPlayerController::ReleaseDodge()
{
	bIsHoldingDodge = false;
}

void AUTPlayerController::OnTapForward()
{
	LastTapBackTime = -10.f;
	LastTapForwardTime = (!UTCharacter || CheckDodge(LastTapForwardTime, true, false, false, false)) ? -10.f : GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapBack()
{
	LastTapForwardTime = -10.f;
	LastTapBackTime = (!UTCharacter || CheckDodge(LastTapBackTime, false, true, false, false)) ? -10.f : GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapLeft()
{
	LastTapRightTime = -10.f;
	LastTapLeftTime = (!UTCharacter || CheckDodge(LastTapLeftTime, false, false, true, false)) ? -10.f : GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapRight()
{
	LastTapLeftTime = -10.f;
	LastTapRightTime = (!UTCharacter || CheckDodge(LastTapRightTime, false, false, false, true)) ? -10.f : GetWorld()->GetTimeSeconds();
}


void AUTPlayerController::UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	Super::UpdateHiddenComponents(ViewLocation, HiddenComponents);

	for (int32 i = RecentWeaponPickups.Num() - 1; i >= 0; i--)
	{
		if (RecentWeaponPickups[i] == NULL)
		{
			RecentWeaponPickups.RemoveAt(i, 1, false);
		}
		else if (!RecentWeaponPickups[i]->IsTaken(GetPawn()))
		{
			RecentWeaponPickups[i]->PlayRespawnEffects();
			RecentWeaponPickups.RemoveAt(i, 1, false);
		}
		else if (RecentWeaponPickups[i]->GetMesh() != NULL)
		{
			HiddenComponents.Add(RecentWeaponPickups[i]->GetMesh()->ComponentId);
		}
	}
}

void AUTPlayerController::SetName(const FString& S)
{
	if (!S.IsEmpty())
	{
		Super::SetName(S);

		UUTGameUserSettings* Settings;
		Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
		if (Settings)
		{
			Settings->SetPlayerName(S);
			Settings->SaveSettings();
		}
	}
}

void AUTPlayerController::ToggleScoreboard(bool bShow)
{
	if (MyHUD != NULL && Cast<AUTHUD>(MyHUD) != NULL)
	{
		Cast<AUTHUD>(MyHUD)->ToggleScoreboard(bShow);
	}
}

void AUTPlayerController::ClientToggleScoreboard_Implementation(bool bShow)
{
	ToggleScoreboard(bShow);
}

void AUTPlayerController::ClientSetHUDAndScoreboard_Implementation(TSubclassOf<class AHUD> NewHUDClass, TSubclassOf<class UUTScoreboard> NewScoreboardClass)
{
	// First, create the HUD

	ClientSetHUD_Implementation(NewHUDClass);

	MyUTHUD = Cast<AUTHUD>(MyHUD);
	if (MyUTHUD != NULL && NewScoreboardClass != NULL)
	{
		MyUTHUD->CreateScoreboard(NewScoreboardClass);
	}
	
}

void AUTPlayerController::OnShowScores()
{
	ToggleScoreboard(true);
}

void AUTPlayerController::OnHideScores()
{
	ToggleScoreboard(false);
}

AUTCharacter* AUTPlayerController::GetUTCharacter()
{
	return UTCharacter;
}

void AUTPlayerController::ServerRestartPlayer_Implementation()
{
	if (!GetWorld()->GetAuthGameMode()->HasMatchStarted() && UTPlayerState != NULL)
	{
		UTPlayerState ->bReadyToPlay = true;
	}

	Super::ServerRestartPlayer_Implementation();

}

bool AUTPlayerController::CanRestartPlayer()
{
	return Super::CanRestartPlayer() && UTPlayerState->RespawnTime <= 0.0f;
}


void AUTPlayerController::BehindView(bool bWantBehindView)
{
	SetCameraMode( (bWantBehindView ? FName(TEXT("FreeCam")) : FName(TEXT("Default"))));
}

bool AUTPlayerController::IsBehindView()
{
	if (PlayerCameraManager != NULL)
	{
		FName CameraStyle = PlayerCameraManager->CameraStyle;
		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		if (GameState != NULL)
		{
			CameraStyle = GameState->OverrideCameraStyle(this, CameraStyle);
		}

		return CameraStyle == FName(TEXT("FreeCam"));
	}
	return false;
}

void AUTPlayerController::ClientSetCameraMode_Implementation( FName NewCamMode )
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->CameraStyle = NewCamMode;
	}

	if (UTCharacter != NULL && IsLocalPlayerController())
	{
		UTCharacter->SetMeshVisibility(NewCamMode == FName(TEXT("FreeCam")));
	}
}

void AUTPlayerController::SetCameraMode( FName NewCamMode )
{
	ClientSetCameraMode_Implementation(NewCamMode);
	
	if ( GetNetMode() == NM_DedicatedServer )
	{
		ClientSetCameraMode( NewCamMode );
	}
}

void AUTPlayerController::ClientGameEnded_Implementation(AActor* EndGameFocus, bool bIsWinner)
{
	ChangeState(FName(TEXT("GameOver")));
	FinalViewTarget = EndGameFocus;
	BehindView(true);
	Super::ClientGameEnded_Implementation(EndGameFocus, bIsWinner);
}

void AUTPlayerController::SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams)
{
	if (FinalViewTarget != NULL)
	{
		NewViewTarget = FinalViewTarget;
	}

	Super::SetViewTarget(NewViewTarget, TransitionParams);
}

void AUTPlayerController::ClientHalftime_Implementation()
{
}



// LEAVE ME for quick debug commands when we need them.
void AUTPlayerController::DebugTest()
{
	UE_LOG(UT,Log,TEXT("DEBUG"));


	ServerDebugTest();
}

void AUTPlayerController::ServerDebugTest_Implementation()
{
	UE_LOG(UT,Log,TEXT("SERVERDEBUG"));

	AUTCTFGameState* CGS = GetWorld()->GetGameState<AUTCTFGameState>();
	if (CGS != NULL)
	{
		UE_LOG(UT,Log,TEXT("GameState: %s"), *GetNameSafe(CGS));
		UE_LOG(UT,Log,TEXT("   FlagBase[0] %s"), *GetNameSafe(CGS->FlagBases[0]));
		if (CGS->FlagBases[0] != NULL)
		{
			UE_LOG(UT,Log,TEXT("       Flag: %s in state %s with holder %s"), *GetNameSafe(CGS->FlagBases[0]->MyFlag), (CGS->FlagBases[0]->MyFlag != NULL ? *CGS->FlagBases[0]->MyFlag->ObjectState.ToString() : TEXT("None")),
										(CGS->FlagBases[0]->MyFlag != NULL ? *GetNameSafe(CGS->FlagBases[0]->MyFlag->Holder) : TEXT("None")));
		}
		UE_LOG(UT,Log,TEXT("   FlagBase[1] %s"), *GetNameSafe(CGS->FlagBases[1]));
		if (CGS->FlagBases[1] != NULL)
		{
			UE_LOG(UT,Log,TEXT("       Flag: %s in state %s with holder %s"), *GetNameSafe(CGS->FlagBases[1]->MyFlag), (CGS->FlagBases[1]->MyFlag != NULL ? *CGS->FlagBases[1]->MyFlag->ObjectState.ToString() : TEXT("None")),
										(CGS->FlagBases[1]->MyFlag != NULL ? *GetNameSafe(CGS->FlagBases[1]->MyFlag->Holder) : TEXT("None")));
		}
	}

}

bool AUTPlayerController::ServerDebugTest_Validate() {return true;}



void AUTPlayerController::PlayerTick( float DeltaTime )
{
	Super::PlayerTick(DeltaTime);
	if (StateName == FName(TEXT("GameOver")))
	{
		UpdateRotation(DeltaTime);
	}
}

void AUTPlayerController::NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent)
{
	APlayerState* InstigatedByState = (InstigatedBy != NULL) ? InstigatedBy->PlayerState : NULL;
	FVector RelHitLocation(FVector::ZeroVector);
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		RelHitLocation = ((FPointDamageEvent*)&DamageEvent)->HitInfo.Location - GetPawn()->GetActorLocation();
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && ((FRadialDamageEvent*)&DamageEvent)->ComponentHits.Num() > 0)
	{
		RelHitLocation = ((FRadialDamageEvent*)&DamageEvent)->ComponentHits[0].Location - GetPawn()->GetActorLocation();
	}
	ClientNotifyTakeHit(InstigatedByState, Damage, Momentum, RelHitLocation, DamageEvent.DamageTypeClass);
}

void AUTPlayerController::ClientNotifyTakeHit_Implementation(APlayerState* InstigatedBy, int32 Damage, FVector Momentum, FVector RelHitLocation, TSubclassOf<UDamageType> DamageType)
{
	if (MyUTHUD != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		MyUTHUD->PawnDamaged(((GetPawn() != NULL) ? GetPawn()->GetActorLocation() : FVector::ZeroVector) + RelHitLocation, Damage, DamageType, InstigatedBy != PlayerState && GS != NULL && GS->OnSameTeam(InstigatedBy, this));
	}
}

void AUTPlayerController::ClientNotifyCausedHit_Implementation(APawn* HitPawn, int32 Damage)
{
	// by default we only show HUD hitconfirms for hits that the player could conceivably see (i.e. target is in LOS)
	if (HitPawn != NULL && HitPawn->GetRootComponent() != NULL && GetPawn() != NULL && MyUTHUD != NULL)
	{
		float VictimLastRenderTime = -1.0f;
		TArray<USceneComponent*> Components;
		HitPawn->GetRootComponent()->GetChildrenComponents(true, Components);
		for (int32 i = 0; i < Components.Num(); i++)
		{
			UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Components[i]);
			if (Prim != NULL)
			{
				VictimLastRenderTime = FMath::Max<float>(VictimLastRenderTime, Prim->LastRenderTime);
			}
		}
		if (GetWorld()->TimeSeconds - VictimLastRenderTime < 0.15f)
		{
			MyUTHUD->CausedDamage(HitPawn, Damage);
		}
	}
}

void AUTPlayerController::ShowMenu()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		LP->ShowMenu();
	}

}

void AUTPlayerController::ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		LP->ShowMessage(MessageTitle, MessageText, Buttons, Callback);
	}	
}


void AUTPlayerController::K2_ReceiveLocalizedMessage(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

uint8 AUTPlayerController::GetTeamNum() const
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	return (PS != NULL && PS->Team != NULL) ? PS->Team->TeamIndex : 255;
}

void AUTPlayerController::ChangeTeam(uint8 NewTeamIndex)
{
	AUTPlayerState* PS = Cast<AUTPlayerState>(PlayerState);
	if (PS != NULL)
	{
		PS->ServerRequestChangeTeam(NewTeamIndex);
	}
}

void AUTPlayerController::Suicide()
{
	ServerSuicide();
}
void AUTPlayerController::ServerSuicide_Implementation()
{
	// throttle suicides to avoid spamming to grief own team in TDM
	if (GetPawn() != NULL && (GetWorld()->TimeSeconds - GetPawn()->CreationTime > 10.0f || GetWorld()->WorldType == EWorldType::PIE || GetNetMode() == NM_Standalone))
	{
		AUTCharacter* Char = Cast<AUTCharacter>(GetPawn());
		if (Char != NULL)
		{
			Char->PlayerSuicide();
		}
	}
}
bool AUTPlayerController::ServerSuicide_Validate()
{
	return true;
}

void AUTPlayerController::Say(FString Message)
{
	ServerSay(Message, false);
}

void AUTPlayerController::TeamSay(FString Message)
{
	ServerSay(Message, true);
}

bool AUTPlayerController::ServerSay_Validate(const FString& Message, bool bTeamMessage) { return true; }

void AUTPlayerController::ServerSay_Implementation(const FString& Message, bool bTeamMessage)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		AUTPlayerController* UTPC = Cast<AUTPlayerController>(*Iterator);
		if (UTPC != nullptr)
		{
			if (!bTeamMessage || UTPC->GetTeamNum() == GetTeamNum())
			{
				UTPC->ClientSay(UTPlayerState, Message, bTeamMessage);
			}
		}
	}
}

void AUTPlayerController::ClientSay_Implementation(AUTPlayerState* Speaker, const FString& Message, bool bTeamMessage)
{
	FClientReceiveData ClientData;
	ClientData.LocalPC = this;
	ClientData.MessageIndex = bTeamMessage;
	ClientData.RelatedPlayerState_1 = Speaker;
	ClientData.MessageString = Message;

	UUTChatMessage::StaticClass()->GetDefaultObject<UUTChatMessage>()->ClientReceive(ClientData);
}


