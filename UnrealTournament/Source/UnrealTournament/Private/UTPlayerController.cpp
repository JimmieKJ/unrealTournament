// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTCharacterMovement.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "UTPickupInventory.h"
#include "UTPickupWeapon.h"
#include "UTAnnouncer.h"
#include "UTHUDWidgetMessage.h"
#include "UTPlayerInput.h"
#include "UTPlayerCameraManager.h"
#include "UTCheatManager.h"
#include "UTCTFGameState.h"
#include "UTChatMessage.h"
#include "Engine/Console.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "Online.h"
#include "UTOnlineGameSearchBase.h"
#include "OnlineSubsystemTypes.h"
#include "UTDroppedPickup.h"
#include "UTGameEngine.h"
#include "UnrealNetwork.h"
#include "UTProfileSettings.h"
#include "UTViewPlaceholder.h"
#include "DataChannel.h"
#include "Engine/GameInstance.h"
#include "UTSpectatorCamera.h"
#include "UTHUDWidget_NetInfo.h"
#include "UTWorldSettings.h"
#include "Engine/DemoNetDriver.h"

DEFINE_LOG_CATEGORY_STATIC(LogUTPlayerController, Log, All);

AUTPlayerController::AUTPlayerController(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	bAutoWeaponSwitch = true;
	
	MaxDodgeClickTime = 0.25f;
	MaxDodgeTapTime = 0.3f;
	LastTapLeftTime = -10.f;
	LastTapRightTime = -10.f;
	LastTapForwardTime = -10.f;
	LastTapBackTime = -10.f;
	bDeferFireInputs = true;
	bSingleTapWallDodge = true;
	bSingleTapAfterJump = true;
	bTapCrouchToSlide = true;
	CrouchSlideTapInterval = 0.25f;
	bHasUsedSpectatingBind = false;
	bAutoCam = true;

	PlayerCameraManagerClass = AUTPlayerCameraManager::StaticClass();
	CheatClass = UUTCheatManager::StaticClass();

	WeaponBobGlobalScaling = 1.f;
	EyeOffsetGlobalScaling = 1.f;

	ConfigDefaultFOV = 100.0f;
	FFAPlayerColor = FLinearColor(0.020845f, 0.335f, 0.0f, 1.0f);

	LastEmoteTime = 0.0f;
	EmoteCooldownTime = 0.3f;

	bAutoSlide = false;

	bSpectateBehindView = true;
	StylizedPPIndex = INDEX_NONE;

	PredictionFudgeFactor = 15.f;
	MaxPredictionPing = 0.f; 
	DesiredPredictionPing = 0.f;

	bIsDebuggingProjectiles = false;
	bUseClassicGroups = true;

	CastingGuideViewIndex = INDEX_NONE;

	static ConstructorHelpers::FObjectFinder<USoundBase> PressedSelect(TEXT("SoundCue'/Game/RestrictedAssets/UI/UT99UI_LittleSelect_Cue.UT99UI_LittleSelect_Cue'"));
	SelectSound = PressedSelect.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> ChatMsgSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/UI/A_UI_Chat01.A_UI_Chat01'"));
	ChatMsgSound = ChatMsgSoundFinder.Object;
}

void AUTPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (Role < ROLE_Authority)
	{
		ServerNegotiatePredictionPing(DesiredPredictionPing);
	}
}

void AUTPlayerController::Destroyed()
{
	Super::Destroyed();

	// if this is the master for the casting guide, destroy the extra viewports now
	if (bCastingGuide)
	{
		ULocalPlayer* LP = Cast<ULocalPlayer>(Player);
		if (LP != NULL && LP->ViewportClient != NULL && LP->GetGameInstance() != NULL)
		{
			TArray<ULocalPlayer*> GamePlayers = LP->GetGameInstance()->GetLocalPlayers();
			for (ULocalPlayer* OtherLP : GamePlayers)
			{
				if (OtherLP != LP)
				{
					LP->GetGameInstance()->RemoveLocalPlayer(OtherLP);
				}
			}
		}
	}
}

void AUTPlayerController::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AUTPlayerController, MaxPredictionPing, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerController, PredictionFudgeFactor, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerController, bAllowPlayingBehindView, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerController, bCastingGuide, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AUTPlayerController, CastingGuideViewIndex, COND_OwnerOnly);
}


void AUTPlayerController::SendPersonalMessage(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	if (GetPawn())
	{
		// send to spectators viewing this pawn as well;
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PC = *Iterator;
			if (PC && PC->PlayerState && PC->PlayerState->bOnlySpectator && (PC->GetViewTarget() == GetPawn()))
			{
				PC->ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
			}
		}
	}
}

void AUTPlayerController::NetStats()
{
	bShowNetInfo = !bShowNetInfo;
	if (MyUTHUD && !MyUTHUD->HasHudWidget(UUTHUDWidget_NetInfo::StaticClass()))
	{
		MyUTHUD->AddHudWidget(UUTHUDWidget_NetInfo::StaticClass());
	}
}

void AUTPlayerController::ServerNegotiatePredictionPing_Implementation(float NewPredictionPing)
{
	MaxPredictionPing = FMath::Clamp(NewPredictionPing, 0.f, UUTGameEngine::StaticClass()->GetDefaultObject<UUTGameEngine>()->ServerMaxPredictionPing);
}

bool AUTPlayerController::ServerNegotiatePredictionPing_Validate(float NewPredictionPing)
{
	return true;
}

void AUTPlayerController::Predict(float NewPredictionPing)
{
	DesiredPredictionPing = NewPredictionPing;
	SaveConfig();
	ServerNegotiatePredictionPing(NewPredictionPing);
}

float AUTPlayerController::GetPredictionTime()
{
	// exact ping is in msec, divide by 1000 to get time in seconds
	//if (Role == ROLE_Authority) { UE_LOG(UT, Warning, TEXT("Server ExactPing %f"), PlayerState->ExactPing); }
	return (PlayerState && (GetNetMode() != NM_Standalone)) ? (0.0005f*FMath::Clamp(PlayerState->ExactPing - PredictionFudgeFactor, 0.f, MaxPredictionPing)) : 0.f;
}

float AUTPlayerController::GetProjectileSleepTime()
{
	return 0.001f * FMath::Max(0.f, PlayerState->ExactPing - PredictionFudgeFactor - MaxPredictionPing);
}

void AUTPlayerController::NP()
{
	ServerNP();
}

bool AUTPlayerController::ServerNP_Validate()
{
	return true;
}

void AUTPlayerController::ServerNP_Implementation()
{
	if (Player && UUTGameEngine::StaticClass()->GetDefaultObject<UUTGameEngine>()->bAllowClientNetProfile)
	{
		Player->Exec(GetWorld(), *FString::Printf(TEXT("NETPROFILE")), *GLog);
	}
}

bool AUTPlayerController::ServerNotifyProjectileHit_Validate(AUTProjectile* HitProj, FVector HitLocation, AActor* DamageCauser, float TimeStamp)
{
	return true;
}

void AUTPlayerController::ServerNotifyProjectileHit_Implementation(AUTProjectile* HitProj, FVector HitLocation, AActor* DamageCauser, float TimeStamp)
{
	// @TODO FIXMESTEVE - need to verify shot from player's location at timestamp to HitLocation is valid, and that projectile should have been there at that time
	if (HitProj)
	{
		HitProj->NotifyClientSideHit(this, HitLocation, DamageCauser);
	}
}

void AUTPlayerController::ToggleSingleTap()
{
	bSingleTapWallDodge = !bSingleTapWallDodge;
}

void AUTPlayerController::ToggleAutoSlide()
{
	SetAutoSlide(!bAutoSlide);
}

void AUTPlayerController::SetAutoSlide(bool bNewAutoSlide)
{
	bAutoSlide = bNewAutoSlide;
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement)
	{
		MyCharMovement->bAutoSlide = bAutoSlide;
	}
	if (Role != ROLE_Authority)
	{
		ServerSetAutoSlide(bAutoSlide);
	}
}

void AUTPlayerController::ServerSetAutoSlide_Implementation(bool bNewAutoSlide)
{
	SetAutoSlide(bNewAutoSlide);
}

bool AUTPlayerController::ServerSetAutoSlide_Validate(bool bNewAutoSlide)
{
	return true;
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
	InputComponent->BindAxis("MoveBackward", this, &AUTPlayerController::MoveBackward);
	InputComponent->BindAxis("MoveLeft", this, &AUTPlayerController::MoveLeft);
	InputComponent->BindAxis("MoveRight", this, &AUTPlayerController::MoveRight);
	InputComponent->BindAxis("MoveUp", this, &AUTPlayerController::MoveUp);
	InputComponent->BindAction("Jump", IE_Pressed, this, &AUTPlayerController::Jump);
	InputComponent->BindAction("Crouch", IE_Pressed, this, &AUTPlayerController::Crouch);
	InputComponent->BindAction("Crouch", IE_Released, this, &AUTPlayerController::UnCrouch);
	InputComponent->BindAction("ToggleCrouch", IE_Pressed, this, &AUTPlayerController::ToggleCrouch);
	InputComponent->BindAction("Slide", IE_Pressed, this, &AUTPlayerController::Slide);

	InputComponent->BindAction("TapLeft", IE_Pressed, this, &AUTPlayerController::OnTapLeft);
	InputComponent->BindAction("TapRight", IE_Pressed, this, &AUTPlayerController::OnTapRight);
	InputComponent->BindAction("TapForward", IE_Pressed, this, &AUTPlayerController::OnTapForward);
	InputComponent->BindAction("TapBack", IE_Pressed, this, &AUTPlayerController::OnTapBack);
	InputComponent->BindAction("SingleTapDodge", IE_Pressed, this, &AUTPlayerController::OnSingleTapDodge);
	InputComponent->BindAction("HoldDodge", IE_Pressed, this, &AUTPlayerController::HoldDodge);
	InputComponent->BindAction("HoldDodge", IE_Released, this, &AUTPlayerController::ReleaseDodge);
	InputComponent->BindAction("HoldRollSlide", IE_Pressed, this, &AUTPlayerController::HoldRollSlide);
	InputComponent->BindAction("HoldRollSlide", IE_Released, this, &AUTPlayerController::ReleaseRollSlide);

	InputComponent->BindAction("TapLeftRelease", IE_Released, this, &AUTPlayerController::OnTapLeftRelease);
	InputComponent->BindAction("TapRightRelease", IE_Released, this, &AUTPlayerController::OnTapRightRelease);
	InputComponent->BindAction("TapForwardRelease", IE_Released, this, &AUTPlayerController::OnTapForwardRelease);
	InputComponent->BindAction("TapBackRelease", IE_Released, this, &AUTPlayerController::OnTapBackRelease);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	InputComponent->BindAxis("Turn", this, &APlayerController::AddYawInput);
	InputComponent->BindAxis("TurnRate", this, &AUTPlayerController::TurnAtRate);
	InputComponent->BindAxis("LookUp", this, &APlayerController::AddPitchInput);
	InputComponent->BindAxis("LookUpRate", this, &AUTPlayerController::LookUpAtRate);

	InputComponent->BindAction("PrevWeapon", IE_Pressed, this, &AUTPlayerController::PrevWeapon);
	InputComponent->BindAction("NextWeapon", IE_Released, this, &AUTPlayerController::NextWeapon);
	InputComponent->BindAction("ThrowWeapon", IE_Released, this, &AUTPlayerController::ThrowWeapon);

	InputComponent->BindAction("StartFire", IE_Pressed, this, &AUTPlayerController::OnFire);
	InputComponent->BindAction("StopFire", IE_Released, this, &AUTPlayerController::OnStopFire);
	InputComponent->BindAction("StartAltFire", IE_Pressed, this, &AUTPlayerController::OnAltFire);
	InputComponent->BindAction("StopAltFire", IE_Released, this, &AUTPlayerController::OnStopAltFire);
	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AUTPlayerController::TouchStarted);

	InputComponent->BindAction("ShowScores", IE_Pressed, this, &AUTPlayerController::OnShowScores);
	InputComponent->BindAction("ShowScores", IE_Released, this, &AUTPlayerController::OnHideScores);

	InputComponent->BindAction("Talk", IE_Pressed, this, &AUTPlayerController::Talk);
	InputComponent->BindAction("TeamTalk", IE_Pressed, this, &AUTPlayerController::TeamTalk);

	InputComponent->BindAction("FasterEmote", IE_Pressed, this, &AUTPlayerController::FasterEmote);
	InputComponent->BindAction("SlowerEmote", IE_Pressed, this, &AUTPlayerController::SlowerEmote);
	InputComponent->BindAction("PlayTaunt", IE_Pressed, this, &AUTPlayerController::PlayTaunt);
	InputComponent->BindAction("PlayTaunt2", IE_Pressed, this, &AUTPlayerController::PlayTaunt2);

	InputComponent->BindAction("ShowBuyMenu", IE_Pressed, this, &AUTPlayerController::ShowBuyMenu);

	UpdateWeaponGroupKeys();
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
		PlayerInput = NewObject<UUTPlayerInput>(this, UUTPlayerInput::StaticClass());
	}

	Super::InitInputSystem();

	if (RewardAnnouncerPath.AssetLongPathname.Len() > 0)
	{
		TSubclassOf<UUTAnnouncer> RewardAnnouncerClass = LoadClass<UUTAnnouncer>(NULL, *RewardAnnouncerPath.AssetLongPathname, NULL, 0, NULL);
		if (RewardAnnouncerClass != NULL && RewardAnnouncerClass.GetDefaultObject()->IsRewardAnnouncer())
		{
			RewardAnnouncer = NewObject<UUTAnnouncer>(this, RewardAnnouncerClass);
		}
	}
	if (StatusAnnouncerPath.AssetLongPathname.Len() > 0)
	{
		TSubclassOf<UUTAnnouncer> StatusAnnouncerClass = LoadClass<UUTAnnouncer>(NULL, *StatusAnnouncerPath.AssetLongPathname, NULL, 0, NULL);
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
				PlayerState->PlayerName = GameMode->DefaultPlayerName.ToString();
			}
		}
	}
}

void AUTPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	UTPlayerState = Cast<AUTPlayerState>(PlayerState);
	if (UTPlayerState != NULL && bCastingGuide)
	{
		OnRep_CastingGuide();
	}
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

	if (Player && IsLocalPlayerController())
	{
		// apply FOV angle if dead/spectating
		if (GetPawn() == NULL && PlayerCameraManager != NULL)
		{
			FOV(ConfigDefaultFOV);
		}
		if (UTCharacter && UTCharacter->UTCharacterMovement)
		{
			UTCharacter->UTCharacterMovement->UpdateSlideRoll(bIsHoldingSlideRoll);
			SetAutoSlide(bAutoSlide);
		}
	}
}

void AUTPlayerController::SpawnPlayerCameraManager()
{
	/** @TODO FIXMESTEVE  - engine crashes, but shouldn't need camera manager on server
	if (GetNetMode() == NM_DedicatedServer)
	{
		// no camera manager on dedicated server
		return;
	}
	*/
	Super::SpawnPlayerCameraManager();
	// init configured FOV angle
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->DefaultFOV = ConfigDefaultFOV;
	}
}

void AUTPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	Super::ClientRestart_Implementation(NewPawn);

	// make sure we don't have leftover zoom
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->UnlockFOV();
		PlayerCameraManager->DefaultFOV = ConfigDefaultFOV;
	}

	if (PlayerState != nullptr)
	{
		Cast<AUTPlayerState>(PlayerState)->RespawnChoiceA = nullptr;
		Cast<AUTPlayerState>(PlayerState)->RespawnChoiceB = nullptr;
	}

	SetCameraMode("Default");

}

void AUTPlayerController::PawnPendingDestroy(APawn* InPawn)
{
	if (IsInState(NAME_Inactive))
	{
		UE_LOG(LogPath, Log, TEXT("PawnPendingDestroy while inactive %s"), *GetName());
	}

	if (InPawn == GetPawn() && InPawn != NULL)
	{
		GetPawn()->UnPossessed();
		SetPawn(NULL);

		FRotator AdjustedCameraRot = GetControlRotation();
		AdjustedCameraRot.Pitch = -45.0f;
		AdjustedCameraRot.Roll = 0.f;
		SetControlRotation(AdjustedCameraRot);

		ChangeState(NAME_Inactive);

		if (PlayerState == NULL)
		{
			Destroy();
		}
	}
}

void AUTPlayerController::FOV(float NewFOV)
{
	if (NewFOV != ConfigDefaultFOV)
	{
		ConfigDefaultFOV = FMath::Clamp<float>(NewFOV, FOV_CONFIG_MIN, FOV_CONFIG_MAX);
		if (PlayerCameraManager != NULL)
		{
			PlayerCameraManager->DefaultFOV = ConfigDefaultFOV;
		}
		if (GetPawn() != NULL && GetNetMode() != NM_Standalone)
		{
			Suicide();
		}
		SaveConfig();
	}
}

void AUTPlayerController::AdvanceStatsPage(int32 Increment)
{
	CurrentlyViewedStatsTab = FMath::Clamp(CurrentlyViewedStatsTab + Increment, 0, 2);
	ServerSetViewedScorePS(CurrentlyViewedScorePS, CurrentlyViewedStatsTab);
}

bool AUTPlayerController::InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	// hack for scoreboard until we have a real interactive system
	if (MyUTHUD != NULL && MyUTHUD->bShowScores && MyUTHUD->GetScoreboard() != NULL && EventType == IE_Pressed)
	{
		static FName NAME_Left(TEXT("Left"));
		static FName NAME_Right(TEXT("Right"));
		static FName NAME_Down(TEXT("Down"));
		static FName NAME_Up(TEXT("Up"));
		if (Key.GetFName() == NAME_Left)
		{
			MyUTHUD->GetScoreboard()->AdvancePage(-1);
			return true;
		}
		else if (Key.GetFName() == NAME_Right)
		{
			MyUTHUD->GetScoreboard()->AdvancePage(+1);
			return true;
		}
		else if (Key.GetFName() == NAME_Up)
		{
			AdvanceStatsPage(-1);
			return true;
		}
		else if (Key.GetFName() == NAME_Down)
		{
			AdvanceStatsPage(+1);
			return true;
		}
	}

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
		for (TInventoryIterator<AUTWeapon> It(UTCharacter); It; ++It)
		{
			if (It->HasAnyAmmo())
			{
				float TestPriority = It->AutoSwitchPriority;
				if (TestPriority > BestPriority)
				{
					BestWeapon = *It;
					BestPriority = TestPriority;
				}
			}
		}
		UTCharacter->SwitchWeapon(BestWeapon);
	}
}
void AUTPlayerController::ClientSwitchToBestWeapon_Implementation()
{
	SwitchToBestWeapon();
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
	if (UTCharacter != NULL && UTCharacter->GetWeapon() != NULL && IsLocalPlayerController())
	{
		int32 CurrentGroup = bUseClassicGroups ? UTCharacter->GetWeapon()->ClassicGroup : UTCharacter->GetWeapon()->Group;
		if (CurrentGroup == 0)
		{
			SwitchWeapon(PreviousWeaponGroup);
		}
		else
		{
			PreviousWeaponGroup = CurrentGroup;
			SwitchWeapon(0);
		}
	}
}

void AUTPlayerController::ThrowWeapon()
{
	if (UTCharacter != NULL && IsLocalPlayerController() && !UTCharacter->IsRagdoll())
	{
		if (UTCharacter->GetWeapon() != nullptr && UTCharacter->GetWeapon()->DroppedPickupClass != nullptr && UTCharacter->GetWeapon()->bCanThrowWeapon)
		{
			ServerThrowWeapon();
		}
	}
}

bool AUTPlayerController::ServerThrowWeapon_Validate()
{
	return true;
}

void AUTPlayerController::ServerThrowWeapon_Implementation()
{
	if (UTCharacter != NULL && !UTCharacter->IsRagdoll())
	{
		if (UTCharacter->GetWeapon() != nullptr && UTCharacter->GetWeapon()->DroppedPickupClass != nullptr && UTCharacter->GetWeapon()->bCanThrowWeapon)
		{
			UTCharacter->TossInventory(UTCharacter->GetWeapon(), FVector(400.0f, 0, 200.f));
		}
	}
}

void AUTPlayerController::SwitchWeaponInSequence(bool bPrev)
{
	if (UTCharacter != NULL && IsLocalPlayerController() && UTCharacter->EmoteCount == 0 && !UTCharacter->IsRagdoll())
	{
		if (UTCharacter->GetWeapon() == NULL)
		{
			SwitchToBestWeapon();
		}
		else
		{
			AUTWeapon* Best = NULL;
			AUTWeapon* WraparoundChoice = NULL;
			AUTWeapon* CurrentWeapon = (UTCharacter->GetPendingWeapon() != NULL) ? UTCharacter->GetPendingWeapon() : UTCharacter->GetWeapon();
			for (TInventoryIterator<AUTWeapon> It(UTCharacter); It; ++It)
			{
				AUTWeapon* Weap = *It;
				if (Weap != CurrentWeapon && Weap->HasAnyAmmo())
				{
					if (Weap->FollowsInList(CurrentWeapon, bUseClassicGroups) == bPrev)
					{
						// remember last weapon in list as possible wraparound choice
						if (WraparoundChoice == NULL || (Weap->FollowsInList(WraparoundChoice, bUseClassicGroups) == bPrev))
						{
							WraparoundChoice = Weap;
						}
					}
					else if (Best == NULL || (Weap->FollowsInList(Best, bUseClassicGroups) == bPrev))
					{
						Best = Weap;
					}
				}
			}
			if (Best == NULL)
			{
				Best = WraparoundChoice;
			}
			//UE_LOG(UT, Warning, TEXT("Switch(previous %d) to %s %d %d"), bPrev, *Best->GetName(), Best->Group, Best->GroupSlot);
			UTCharacter->SwitchWeapon(Best);
		}
	}
	else if (PlayerState && PlayerState->bIsSpectator && PlayerCameraManager)
	{
		float Offset = 10000.f * GetWorld()->GetDeltaSeconds();
		if (bPrev)
		{
			Offset *= -1.f;
		}
		ASpectatorPawn* Spectator = Cast<ASpectatorPawn>(GetViewTarget());
		if (!Spectator)
		{
			PlayerCameraManager->FreeCamDistance = FMath::Clamp(PlayerCameraManager->FreeCamDistance + Offset, 16.f, 2048.f);
		}
		else
		{
			USpectatorPawnMovement* SpectatorMovement = Cast<USpectatorPawnMovement>(Spectator->GetMovementComponent());
			if (SpectatorMovement)
			{
				SpectatorMovement->MaxSpeed = FMath::Clamp(SpectatorMovement->MaxSpeed + 5.f*Offset, 200.f, 6000.f);
			}
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
		if (CurWeapon == NULL || (bAutoWeaponSwitch && !UTCharacter->IsPendingFire(CurWeapon->GetCurrentFireMode()) && TestWeapon->AutoSwitchPriority > CurWeapon->AutoSwitchPriority))
		{
			UTCharacter->SwitchWeapon(TestWeapon);
		}
	}
}

void AUTPlayerController::SwitchWeaponGroup(int32 Group)
{
	if (bUseClassicGroups)
	{
		SwitchWeapon(Group);
	}
	if (UTCharacter != NULL && IsLocalPlayerController() && UTCharacter->EmoteCount == 0 && !UTCharacter->IsRagdoll())
	{
		// if current weapon isn't in the specified group, pick lowest GroupSlot in that group
		// if it is, then pick next highest slot, or wrap around to lowest if no higher slot
		AUTWeapon* CurrWeapon = (UTCharacter->GetPendingWeapon() != NULL) ? UTCharacter->GetPendingWeapon() : UTCharacter->GetWeapon();
		AUTWeapon* LowestSlotWeapon = NULL;
		AUTWeapon* NextSlotWeapon = NULL;
		for (TInventoryIterator<AUTWeapon> It(UTCharacter); It; ++It)
		{
			AUTWeapon* Weap = *It;
			if (Weap != UTCharacter->GetWeapon() && Weap->HasAnyAmmo())
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

void AUTPlayerController::SwitchWeapon(int32 Group)
{
	if (UTCharacter != NULL && IsLocalPlayerController() && UTCharacter->EmoteCount == 0 && !UTCharacter->IsRagdoll())
	{
		// if current weapon isn't in the specified group, pick lowest GroupSlot in that group
		// if it is, then pick next highest slot, or wrap around to lowest if no higher slot
		AUTWeapon* CurrWeapon = (UTCharacter->GetPendingWeapon() != NULL) ? UTCharacter->GetPendingWeapon() : UTCharacter->GetWeapon();
		AUTWeapon* LowestSlotWeapon = NULL;
		AUTWeapon* NextSlotWeapon = NULL;
		for (TInventoryIterator<AUTWeapon> It(UTCharacter); It; ++It)
		{
			AUTWeapon* Weap = *It;
			if (Weap != UTCharacter->GetWeapon() && Weap->HasAnyAmmo())
			{
				if (Weap->ClassicGroup == Group)
				{
					if (LowestSlotWeapon == NULL || LowestSlotWeapon->GroupSlot > Weap->GroupSlot)
					{
						LowestSlotWeapon = Weap;
					}
					if (CurrWeapon != NULL && CurrWeapon->ClassicGroup == Group && Weap->GroupSlot > CurrWeapon->GroupSlot && (NextSlotWeapon == NULL || NextSlotWeapon->GroupSlot > Weap->GroupSlot))
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

void AUTPlayerController::DemoRestart()
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		DemoDriver->GotoTimeInSeconds(0);
	}
}

void AUTPlayerController::DemoSeek(float DeltaSeconds)
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		DemoDriver->GotoTimeInSeconds(DemoDriver->DemoCurrentTime + DeltaSeconds);
	}
}

void AUTPlayerController::DemoGoToLive()
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		DemoDriver->JumpToEndOfLiveReplay();
	}
}

void AUTPlayerController::ViewPlayerNum(int32 Index, uint8 TeamNum)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		APlayerState** PlayerToView = NULL;
		if (TeamNum == 255 || GS->Teams.Num() == 0)
		{
			// hack for default binds
			if (TeamNum == 1)
			{
				Index += TeamNum * 5;
			}
			int32 MaxSpectatingId = GS->GetMaxSpectatingId();
			while ((Index <= MaxSpectatingId) && (PlayerToView == NULL))
			{
				PlayerToView = GS->PlayerArray.FindByPredicate([=](const APlayerState* TestItem) -> bool
				{
					const AUTPlayerState* PS = Cast<AUTPlayerState>(TestItem);
					return (PS != NULL && PS->SpectatingID == Index);
				});
				Index += 5;
			}
		}
		else if (GS->Teams.IsValidIndex(TeamNum))
		{
			int32 MaxSpectatingId = GS->GetMaxTeamSpectatingId(TeamNum);
			while ((Index <= MaxSpectatingId) && (PlayerToView == NULL))
			{
				PlayerToView = GS->PlayerArray.FindByPredicate([=](const APlayerState* TestItem) -> bool
				{
					const AUTPlayerState* PS = Cast<AUTPlayerState>(TestItem);
					return (PS != NULL && PS->SpectatingIDTeam == Index && PS->GetTeamNum() == TeamNum);
				});
				Index += 5;
			}
		}
		if (PlayerToView != NULL)
		{
			bAutoCam = false;
			BehindView(bSpectateBehindView);
			ViewPlayerState(*PlayerToView);
		}
	}
}

void AUTPlayerController::ToggleBehindView()
{
	bSpectateBehindView = !bSpectateBehindView;
	BehindView(bSpectateBehindView);

	AUTCarriedObject* UTFlag = Cast<AUTCarriedObject>(GetViewTarget());
	if (!bSpectateBehindView && UTFlag && UTFlag->Holder)
	{
		ServerViewFlagHolder(UTFlag->GetTeamNum());
	}
}

void AUTPlayerController::ToggleSlideOut()
{
	bRequestingSlideOut = !bRequestingSlideOut;
}

void AUTPlayerController::ToggleShowBinds()
{
	bShowCameraBinds = !bShowCameraBinds;
}

void AUTPlayerController::ViewNextPlayer()
{
	bAutoCam = false;
	BehindView(bSpectateBehindView);
	ServerViewNextPlayer();
}

bool AUTPlayerController::ServerViewFlagHolder_Validate(uint8 TeamIndex)
{
	return true;
}
void AUTPlayerController::ServerViewFlagHolder_Implementation(uint8 TeamIndex)
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		AUTCTFGameState* CTFGameState = GetWorld()->GetGameState<AUTCTFGameState>();
		if (CTFGameState && (CTFGameState->FlagBases.Num() > TeamIndex) && CTFGameState->FlagBases[TeamIndex] && CTFGameState->FlagBases[TeamIndex]->MyFlag && CTFGameState->FlagBases[TeamIndex]->MyFlag->Holder)
		{
			SetViewTarget(CTFGameState->FlagBases[TeamIndex]->MyFlag->Holder);
		}
	}
}

void AUTPlayerController::ViewClosestVisiblePlayer()
{
	AUTPlayerState* BestChar = NULL;
	float BestDist = 200000.f;
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		AUTCharacter *UTChar = Cast<AUTCharacter>(*It);
		if (UTChar && (UTChar->Health > 0) && Cast<AUTPlayerState>(UTChar->PlayerState) && (GetWorld()->GetTimeSeconds() - UTChar->GetLastRenderTime() < 0.1f))
		{
			float NewDist = (UTChar->GetActorLocation() - GetViewTarget()->GetActorLocation()).Size();
			if (!BestChar || (NewDist < BestDist))
			{
				BestChar = Cast<AUTPlayerState>(UTChar->PlayerState);
				BestDist = NewDist;
			}
		}
	}
	if (BestChar)
	{
		bAutoCam = false;
		ViewPlayerState(BestChar);
	}
}

void AUTPlayerController::ViewPlayerState(APlayerState* PS)
{
	ServerViewPlayerState(PS);
}

bool AUTPlayerController::ServerViewPlayerState_Validate(APlayerState* PS)
{
	return true;
}

void AUTPlayerController::ServerViewPlayerState_Implementation(APlayerState* PS)
{
	if (PlayerState && PlayerState->bOnlySpectator && PS)
	{
		SetViewTarget(PS);
	}
}

void AUTPlayerController::ViewFlag(uint8 Index)
{
	bAutoCam = false;
	ServerViewFlag(Index);
}

bool AUTPlayerController::ServerViewFlag_Validate(uint8 Index)
{
	return true;
}

void AUTPlayerController::ServerViewFlag_Implementation(uint8 Index)
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		AUTCTFGameState* CTFGameState = GetWorld()->GetGameState<AUTCTFGameState>();
		if (CTFGameState && (CTFGameState->FlagBases.Num() > Index) && CTFGameState->FlagBases[Index] && CTFGameState->FlagBases[Index]->MyFlag )
		{
			SetViewTarget(CTFGameState->FlagBases[Index]->MyFlag);
		}
	}
}

void AUTPlayerController::ViewCamera(int32 Index)
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		int32 CamCount = 0;
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTSpectatorCamera* Cam = Cast<AUTSpectatorCamera>(*It);
			if (Cam)
			{
				CamCount++;
				if (CamCount == Index)
				{
					bAutoCam = false;
					AActor* NewViewTarget = (GetSpectatorPawn() != NULL) ? GetSpectatorPawn() : SpawnSpectatorPawn();
					NewViewTarget->SetActorLocationAndRotation(Cam->GetActorLocation(), Cam->GetActorRotation());
					ResetCameraMode();
					SetViewTarget(NewViewTarget);
					SetControlRotation(Cam->GetActorRotation());
					ServerViewSelf();
				}
			}
		}
	}
}

void AUTPlayerController::ViewProjectile()
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		if (Cast<AUTProjectile>(GetViewTarget()) && LastSpectatedPlayerState)
		{
			// toggle away from projectile cam
			for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
			{
				APawn* Pawn = *Iterator;
				if (Pawn != nullptr && Pawn->PlayerState == LastSpectatedPlayerState)
				{
					bAutoCam = false;
					ServerViewPawn(*Iterator);
				}
			}
		}
		else
		{
			if (LastSpectatedPlayerState == NULL)
			{
				// make sure we have something to go to when projectile explodes
				AUTCharacter* ViewedCharacter = Cast<AUTCharacter>(GetViewTarget());
				if (!ViewedCharacter)
				{
					// maybe viewing character carrying flag
					AUTCarriedObject* Flag = Cast<AUTCarriedObject>(GetViewTarget());
					ViewedCharacter = Flag ? Flag->HoldingPawn : NULL;
				}
				if (ViewedCharacter)
				{
					LastSpectatedPlayerState = ViewedCharacter->PlayerState;
				}
			}
			bAutoCam = false;
			ServerViewProjectile();
		}
	}
}

bool AUTPlayerController::ServerViewProjectile_Validate()
{
	return true;
}

void AUTPlayerController::ServerViewProjectile_Implementation()
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		AUTCharacter* ViewedCharacter = Cast<AUTCharacter>(GetViewTarget());
		if (!ViewedCharacter)
		{
			// maybe viewing character carrying flag
			AUTCarriedObject* Flag = Cast<AUTCarriedObject>(GetViewTarget());
			ViewedCharacter = Flag ? Flag->HoldingPawn : NULL;
		}
		// @TODO FIXMESTEVE save last fired projectile as optimization
		AUTProjectile* BestProj = NULL;
		if (ViewedCharacter)
		{
			for (FActorIterator It(GetWorld()); It; ++It)
			{
				AUTProjectile* Proj = Cast<AUTProjectile>(*It);
				if (Proj && !Proj->bExploded && !Proj->GetVelocity().IsNearlyZero() && (Proj->Instigator == ViewedCharacter) && (!BestProj || (BestProj->CreationTime < Proj->CreationTime)))
				{
					BestProj = Proj;
				}
			}
		}
		if (BestProj)
		{
			SetViewTarget(BestProj);
		}
	}
}

void AUTPlayerController::PlayMenuSelectSound()
{
	if (GetViewTarget())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), SelectSound, GetViewTarget()->GetActorLocation(), 1.f, 1.0f, 0.0f);
	}
}

void AUTPlayerController::OnFire()
{
	if (GetPawn() != NULL)
	{
		if (bDeferFireInputs)
		{
			new(DeferredFireInputs)FDeferredFireInput(0, true);
		}
		else
		{
			if (UTCharacter != NULL)
			{
				if (StateName == NAME_Playing)
				{
					UTCharacter->StartFire(0);
				}
			}
			else if (GetPawn() != nullptr)
			{
				GetPawn()->PawnStartFire(0);
			}
		}
	}
	else if (IsInState(NAME_Spectating))
	{
		PlayMenuSelectSound();
		if ((PlayerState == nullptr || !PlayerState->bOnlySpectator) && 
			bPlayerIsWaiting)
		{
			ServerRestartPlayer();
		}
	}
	else
	{
		PlayMenuSelectSound();
		ServerRestartPlayer();
	}
}

void AUTPlayerController::OnStopFire()
{
	if (GetPawn() != NULL)
	{
		if (bDeferFireInputs)
		{
			new(DeferredFireInputs)FDeferredFireInput(0, false);
		}
		else
		{
			if (UTCharacter != NULL)
			{
				UTCharacter->StopFire(0);
			}
		}
	}
}

void AUTPlayerController::OnAltFire()
{
	if (GetPawn() != NULL)
	{
		if (bDeferFireInputs)
		{
			new(DeferredFireInputs)FDeferredFireInput(1, true);
		}
		else
		{
			if (UTCharacter != NULL)
			{
				if (StateName == NAME_Playing)
				{
					UTCharacter->StartFire(1);
				}
			}
			else if (GetPawn() != nullptr)
			{
				GetPawn()->PawnStartFire(1);
			}
		}
	}
	else if ( GetWorld()->GetGameState<AUTGameState>()->HasMatchStarted() && IsInState(NAME_Spectating) )
	{
		PlayMenuSelectSound();
		if ((PlayerState == nullptr || !PlayerState->bOnlySpectator) && bPlayerIsWaiting)
		{
			ServerRestartPlayer();
		}
		else
		{
			bAutoCam = false;
			ViewSelf();
		}
	}
	else
	{
		PlayMenuSelectSound();
		ServerRestartPlayerAltFire();
	}
}

void AUTPlayerController::OnStopAltFire()
{
	if (GetPawn() != NULL)
	{
		if (bDeferFireInputs)
		{
			new(DeferredFireInputs)FDeferredFireInput(1, false);
		}
		else
		{
			if (UTCharacter != NULL)
			{
				UTCharacter->StopFire(1);
			}
		}
	}
}

void AUTPlayerController::MoveForward(float Value)
{
	if (Value != 0.0f && UTCharacter != NULL)
	{
		MovementForwardAxis = Value;
		UTCharacter->MoveForward(Value);
	}
	else if (GetSpectatorPawn() != NULL)
	{
		GetSpectatorPawn()->MoveForward(Value);
	}
}

void AUTPlayerController::MoveBackward(float Value)
{
	MoveForward(Value * -1);
}

void AUTPlayerController::MoveLeft(float Value)
{
	MoveRight(Value * -1);
}

void AUTPlayerController::MoveRight(float Value)
{
	if (Value != 0.0f && UTCharacter != NULL)
	{
		MovementStrafeAxis = Value;
		UTCharacter->MoveRight(Value);
	}
	else if (GetSpectatorPawn() != NULL)
	{
		GetSpectatorPawn()->MoveRight(Value);
	}
}

void AUTPlayerController::MoveUp(float Value)
{
	if (Value != 0.0f && UTCharacter != NULL)
	{
		UTCharacter->MoveUp(Value);
	}
	else if (GetSpectatorPawn() != NULL)
	{
		GetSpectatorPawn()->MoveUp_World(Value);
	}
}

void AUTPlayerController::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AUTPlayerController::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AUTPlayerController::Jump()
{
	if (GetCharacter() != NULL)
	{
		GetCharacter()->bPressedJump = true;
	}
}

void AUTPlayerController::Slide()
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement)
	{
		MyCharMovement->bPressedSlide = true;
	}
}

void AUTPlayerController::Crouch()
{
	if (GetCharacter() != NULL)
	{
		GetCharacter()->Crouch(false);
	}
	SlideTapThresholdTime = GetWorld()->GetTimeSeconds() + CrouchSlideTapInterval;
}

void AUTPlayerController::UnCrouch()
{
	if (GetCharacter() != NULL)
	{
		GetCharacter()->UnCrouch(false);
		if (bTapCrouchToSlide && (GetWorld()->GetTimeSeconds() < SlideTapThresholdTime) && (UTCharacter != NULL))
		{
			SlideTapThresholdTime = 0.f;
			Slide();
		}
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
			if (GEngine->GetMainAudioDevice() != NULL)
			{
				GEngine->GetMainAudioDevice()->AddNewActiveSound(NewActiveSound);
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
				AttenuationOverride->Attenuation.dBAttenuationAtMax = 50.0f;
				// move sound closer
				AActor* ViewTarget = GetViewTarget();
				if (ViewTarget != NULL)
				{
					if (SoundLocation.IsZero())
					{
						SoundLocation = SoundPlayer->GetActorLocation();
					}
					FVector SoundOffset = GetViewTarget()->GetActorLocation() - SoundLocation;
					SoundLocation = SoundLocation + SoundOffset.GetSafeNormal() * FMath::Min<float>(SoundOffset.Size() * 0.25f, 2000.0f);
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

void AUTPlayerController::CheckDodge(float LastTapTime, float MaxClickTime, bool bForward, bool bBack, bool bLeft, bool bRight)
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement && (bIsHoldingDodge || (GetWorld()->GetTimeSeconds() - LastTapTime < MaxClickTime)))
	{
		MyCharMovement->bPressedDodgeForward = bForward;
		MyCharMovement->bPressedDodgeBack = bBack;
		MyCharMovement->bPressedDodgeLeft = bLeft;
		MyCharMovement->bPressedDodgeRight = bRight;
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

		if (MovementStrafeAxis > 0.5f)
		{
			MyCharMovement->bPressedDodgeRight = true;
		}
		else if (MovementStrafeAxis < -0.5f)
		{
			MyCharMovement->bPressedDodgeLeft = true;
		}
		else if ( MovementForwardAxis >= 0.f)
		{
			MyCharMovement->bPressedDodgeForward = true;
		}
		else
		{
			MyCharMovement->bPressedDodgeBack = true;
		}
	}
}

void AUTPlayerController::HoldRollSlide()
{
	bIsHoldingSlideRoll = true;
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement)
	{
		MyCharMovement->UpdateSlideRoll(true);
	}
}

void AUTPlayerController::ReleaseRollSlide()
{
	bIsHoldingSlideRoll = false;
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement)
	{
		MyCharMovement->UpdateSlideRoll(false);
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
	LastTapRightTime = -10.f;
	LastTapLeftTime = -10.f;
	CheckDodge(LastTapForwardTime, MaxDodgeClickTime, true, false, false, false);
	LastTapForwardTime = GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapBack()
{
	LastTapForwardTime = -10.f;
	LastTapRightTime = -10.f;
	LastTapLeftTime = -10.f;
	CheckDodge(LastTapBackTime, MaxDodgeClickTime, false, true, false, false);
	LastTapBackTime = GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapLeft()
{
	LastTapRightTime = -10.f;
	LastTapBackTime = -10.f;
	LastTapForwardTime = -10.f;
	CheckDodge(LastTapLeftTime, MaxDodgeClickTime, false, false, true, false);
	LastTapLeftTime = GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapRight()
{
	LastTapLeftTime = -10.f;
	LastTapBackTime = -10.f;
	LastTapForwardTime = -10.f;
	CheckDodge(LastTapRightTime, MaxDodgeClickTime, false, false, false, true);
	LastTapRightTime = GetWorld()->GetTimeSeconds();
}

void AUTPlayerController::OnTapForwardRelease()
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement && bSingleTapWallDodge && !MyCharMovement->IsMovingOnGround() && (!bSingleTapAfterJump || MyCharMovement->bExplicitJump))
	{
		CheckDodge(LastTapForwardTime, MaxDodgeTapTime, true, false, false, false);
	}
}

void AUTPlayerController::OnTapBackRelease()
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement && bSingleTapWallDodge && !MyCharMovement->IsMovingOnGround() && (!bSingleTapAfterJump || MyCharMovement->bExplicitJump))
	{
		CheckDodge(LastTapBackTime, MaxDodgeTapTime, false, true, false, false);
	}
}

void AUTPlayerController::OnTapLeftRelease()
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement && bSingleTapWallDodge && !MyCharMovement->IsMovingOnGround() && (!bSingleTapAfterJump || MyCharMovement->bExplicitJump))
	{
		CheckDodge(LastTapLeftTime, MaxDodgeTapTime, false, false, true, false);
	}
}

void AUTPlayerController::OnTapRightRelease()
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement && bSingleTapWallDodge && !MyCharMovement->IsMovingOnGround() && (!bSingleTapAfterJump || MyCharMovement->bExplicitJump))
	{
		CheckDodge(LastTapRightTime, MaxDodgeTapTime, false, false, false, true);
	}
}

static void HideComponentTree(const UPrimitiveComponent* Primitive, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	if (Primitive != NULL)
	{
		HiddenComponents.Add(Primitive->ComponentId);
		TArray<USceneComponent*> Children;
		Primitive->GetChildrenComponents(true, Children);
		for (int32 i = 0; i < Children.Num(); i++)
		{
			UPrimitiveComponent* ChildPrim = Cast<UPrimitiveComponent>(Children[i]);
			if (ChildPrim != NULL)
			{
				HiddenComponents.Add(ChildPrim->ComponentId);
			}
		}
	}
}

void AUTPlayerController::UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents)
{
	Super::UpdateHiddenComponents(ViewLocation, HiddenComponents);

	AUTWorldSettings* WS = Cast<AUTWorldSettings>(GetWorld()->GetWorldSettings());
	if (WS != NULL)
	{
		for (AUTPickupWeapon* Pickup : WS->WeaponPickups)
		{
			bool bTaken = !Pickup->State.bActive;

			if (RecentWeaponPickups.Contains(Pickup))
			{
				if (Pickup->IsTaken(GetPawn()))
				{
					bTaken = true;
				}
				else
				{
					Pickup->PlayRespawnEffects();
					RecentWeaponPickups.Remove(Pickup);
				}
			}
			if (bTaken)
			{
				if (Pickup->GetMesh() != NULL)
				{
					HiddenComponents.Add(Pickup->GetMesh()->ComponentId);
				}
			}
			else
			{
				if (Pickup->GetGhostMesh() != NULL)
				{
					HiddenComponents.Add(Pickup->GetGhostMesh()->ComponentId);
				}
				if (Pickup->GetGhostDepthMesh() != NULL)
				{
					HiddenComponents.Add(Pickup->GetGhostDepthMesh()->ComponentId);
				}
			}
		}
	}

	// hide all components that shouldn't be shown in the current 1P/3P state
	// with bOwnerNoSee/bOnlyOwnerSee not being propagated to children this method is much easier to maintain
	// although slightly less efficient
	AUTCharacter* P = Cast<AUTCharacter>(GetViewTarget());
	if (IsBehindView())
	{
		// hide first person weapon
		if (P != NULL && P->GetWeapon() != NULL)
		{
			TArray<UMeshComponent*> Meshes = P->GetWeapon()->Get1PMeshes();
			for (UMeshComponent* WeapMesh : Meshes)
			{
				HideComponentTree(WeapMesh, HiddenComponents);
			}
		}
	}
	else if (P != NULL)
	{
		// hide first person mesh (but not attachments) if hidden weapons
		if (GetWeaponHand() == HAND_Hidden && P->GetWeapon() != NULL)
		{
			TArray<UMeshComponent*> Meshes = P->GetWeapon()->Get1PMeshes();
			for (UMeshComponent* WeapMesh : Meshes)
			{
				HiddenComponents.Add(WeapMesh->ComponentId);
			}
		}
		// hide third person character model
		HideComponentTree(P->GetMesh(), HiddenComponents);
		// hide flag
		// TODO: long term would be nice to not do this and have it visible at the edge of vision
		if (P->GetCarriedObject() != NULL)
		{
			HideComponentTree(Cast<UPrimitiveComponent>(P->GetCarriedObject()->GetRootComponent()), HiddenComponents);
		}
	}
	else if (GetViewTarget() != NULL)
	{
		// for others we can't just hide everything because we don't know where the camera component is and we don't want to hide its attachments
		// so just hide root
		UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(GetViewTarget()->GetRootComponent());
		if (RootPrim != NULL)
		{
			HiddenComponents.Add(RootPrim->ComponentId);
		}
	}

	// hide other local players' first person weapons
	for (FLocalPlayerIterator It(GEngine, GetWorld()); It; ++It)
	{
		if (It->PlayerController != this && It->PlayerController != NULL)
		{
			AUTCharacter* OtherP = Cast<AUTCharacter>(It->PlayerController->GetViewTarget());
			if (OtherP != NULL && OtherP->GetWeapon() != NULL)
			{
				HideComponentTree(OtherP->GetWeapon()->Mesh, HiddenComponents);
			}
		}
	}
}

void AUTPlayerController::SetName(const FString& S)
{
	if (!S.IsEmpty())
	{
		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
		if (LP != NULL)
		{
			LP->SetNickname(S);
			LP->SaveProfileSettings();
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
	if (UTPlayerState != nullptr)
	{
		UTPlayerState->bChosePrimaryRespawnChoice = true;
		UTPlayerState->ForceNetUpdate();
	}

	if (!GetWorld()->GetAuthGameMode()->HasMatchStarted())
	{
		if (UTPlayerState)
		{
			UTPlayerState->bReadyToPlay = !UTPlayerState->bReadyToPlay;
			UTPlayerState->bPendingTeamSwitch = false;
			UTPlayerState->ForceNetUpdate();
		}
	}
	else if (IsFrozen())
	{
		return;
	}
	else if (!GetWorld()->GetAuthGameMode()->PlayerCanRestart(this))
	{
		return;
	}

	Super::ServerRestartPlayer_Implementation();
}

bool AUTPlayerController::ServerRestartPlayerAltFire_Validate()
{
	return true;
}

void AUTPlayerController::ServerRestartPlayerAltFire_Implementation()
{
	if (UTPlayerState != nullptr)
	{
		UTPlayerState->bChosePrimaryRespawnChoice = false;
		UTPlayerState->ForceNetUpdate();
	}

	if (!GetWorld()->GetAuthGameMode()->HasMatchStarted())
	{
		if (UTPlayerState && UTPlayerState->Team && (UTPlayerState->Team->TeamIndex < 2))
		{
			ChangeTeam(1 - UTPlayerState->Team->TeamIndex);
			if (UTPlayerState->bPendingTeamSwitch)
			{
				UTPlayerState->bReadyToPlay = false;
				UTPlayerState->ForceNetUpdate();
			}
		}
	}
	else 
	{
		AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (GameMode)
		{
			if (!GameMode->PlayerCanAltRestart(this))
			{
				return;
			}
		}
	}

	Super::ServerRestartPlayer_Implementation();
}

bool AUTPlayerController::CanRestartPlayer()
{
	return Super::CanRestartPlayer() && UTPlayerState->RespawnTime <= 0.0f && (bShortConnectTimeOut || GetWorld()->TimeSeconds - CreationTime > 15.0f ||(GetNetMode() == NM_Standalone));
}

void AUTPlayerController::BehindView(bool bWantBehindView)
{
	if (GetPawn() != NULL && !GetPawn()->bTearOff && !bAllowPlayingBehindView && GetNetMode() != NM_Standalone && (GetWorld()->WorldType != EWorldType::PIE))
	{
		bWantBehindView = false;
	}
	if (IsInState(NAME_Spectating))
	{
		bSpectateBehindView = bWantBehindView;
	}
	SetCameraMode(bWantBehindView ? FName(TEXT("FreeCam")) : FName(TEXT("Default")));
	if (Cast<AUTCharacter>(GetViewTarget()) != NULL)
	{
		((AUTCharacter*)GetViewTarget())->BehindViewChange(this, bWantBehindView);
	}
}

bool AUTPlayerController::IsBehindView()
{
	if (PlayerCameraManager != NULL)
	{
		static FName NAME_FreeCam(TEXT("FreeCam"));

		AUTPlayerCameraManager* UTCam = Cast<AUTPlayerCameraManager>(PlayerCameraManager);
		FName CameraStyle = (UTCam != NULL) ? UTCam->GetCameraStyleWithOverrides() : PlayerCameraManager->CameraStyle;

		return CameraStyle == NAME_FreeCam;
	}
	else
	{
		return false;
	}
}

void AUTPlayerController::ClientSetCameraMode_Implementation( FName NewCamMode )
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->CameraStyle = NewCamMode;
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

void AUTPlayerController::ToggleTacCom()
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		bTacComView = !bTacComView;

		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTPickup* Pickup = Cast<AUTPickup>(*It);
			if (Pickup)
			{
				Pickup->SetTacCom(bTacComView);
			}
		}
		UpdateTacComOverlays();
	}
}

void AUTPlayerController::EnableAutoCam()
{
	bAutoCam = true;
}

void AUTPlayerController::UpdateTacComOverlays()
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter *UTChar = Cast<AUTCharacter>(*It);
			if (UTChar && !UTChar->IsRagdoll())
			{
				UTChar->UpdateTacComMesh(bTacComView);
			}
		}
	}
}

void AUTPlayerController::SetStylizedPP(int32 NewPP)
{
	AUTPlayerCameraManager* UTPCM = Cast<AUTPlayerCameraManager>(PlayerCameraManager);
	if (UTPCM)
	{
		if (NewPP == INDEX_NONE || NewPP < UTPCM->StylizedPPSettings.Num())
		{
			StylizedPPIndex = NewPP;
		}
	}
}

void AUTPlayerController::ClientGameEnded_Implementation(AActor* EndGameFocus, bool bIsWinner)
{
	ChangeState(FName(TEXT("GameOver")));
	FinalViewTarget = EndGameFocus;
	BehindView(true);
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AUTPlayerController::ShowEndGameScoreboard, 10.f, false);
	Super::ClientGameEnded_Implementation(EndGameFocus, bIsWinner);

	// freeze all Pawns locally
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (It->IsValid() && !Cast<ASpectatorPawn>(It->Get()))
		{
			It->Get()->TurnOff();
		}
	}
}

void AUTPlayerController::ShowEndGameScoreboard()
{
	if (MyUTHUD != NULL)
	{
		MyUTHUD->ToggleScoreboard(true);
	}
}

void AUTPlayerController::SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams)
{
	// remove any FOV shifts when changing targets (e.g. sniper zoom)
	if (PlayerCameraManager != NULL)
	{
		PlayerCameraManager->UnlockFOV();
	}

	if (FinalViewTarget != NULL)
	{
		NewViewTarget = FinalViewTarget;
	}

	AUTViewPlaceholder *UTPlaceholder = Cast<AUTViewPlaceholder>(GetViewTarget());
	Super::SetViewTarget(NewViewTarget, TransitionParams);

	// See if we're no longer viewing a placeholder and destroy it
	AActor* UpdatedViewTarget = GetViewTarget();
	if (UTPlaceholder != nullptr && UpdatedViewTarget != UTPlaceholder)
	{
		UTPlaceholder->Destroy();
	}

	if (StateName == NAME_Spectating)
	{
		AUTCharacter* Char = Cast<AUTCharacter>(UpdatedViewTarget);
		if (Char)
		{
			LastSpectatedPlayerState = Char->PlayerState;
		}
		else if (!Cast<AUTProjectile>(UpdatedViewTarget) && (UpdatedViewTarget != this))
		{
			LastSpectatedPlayerState = NULL;
		}

		// FIXME: HACK: PlayerState->bOnlySpectator check is workaround for bug possessing new Pawn where we are actually in the spectating state for a short time after getting the new pawn as viewtarget
		//				happens because Pawn is replicated via property replication and ViewTarget is RPC'ed so comes first
		if (IsLocalController() && bSpectateBehindView && PlayerState && PlayerState->bOnlySpectator && (NewViewTarget != GetSpectatorPawn()))
		{
			// pick good starting rotation
			FindGoodView(false);
		}
	}
}

FRotator AUTPlayerController::GetSpectatingRotation(float DeltaTime)
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS && (!GS->IsMatchInProgress() || GS->IsMatchAtHalftime()))
		{
			return GetControlRotation();
		}
		float OldYaw = FMath::UnwindDegrees(GetControlRotation().Yaw);
		FindGoodView(true);
		FRotator NewRotation = GetControlRotation();
		float NewYaw = FMath::UnwindDegrees(NewRotation.Yaw);
		if (FMath::Abs(NewYaw - OldYaw) < 60.f)
		{
			NewRotation.Yaw = (1.f - 7.f*DeltaTime)*OldYaw + 7.f*DeltaTime*NewYaw;
			SetControlRotation(NewRotation);
		}
	}
	return GetControlRotation();
}

void AUTPlayerController::FindGoodView(bool bIsUpdate)
{
	AActor* TestViewTarget = GetViewTarget();
	if (!TestViewTarget || !PlayerCameraManager || (TestViewTarget == this) || (TestViewTarget == GetSpectatorPawn()) || Cast<AUTProjectile>(TestViewTarget) || Cast<AUTViewPlaceholder>(TestViewTarget))
	{
		// no rotation change;
		return;
	}

	FRotator BestRot = GetControlRotation();
	// Always start looking down slightly
	BestRot.Pitch = -10.f;
	BestRot.Roll = 0.f;
	BestRot.Yaw = FMath::UnwindDegrees(BestRot.Yaw);
	float CurrentYaw = BestRot.Yaw;

	// @TODO FIXMESTEVE - if update, work harder to stay close to current view, slowly move back to behind
	BestRot.Yaw = TestViewTarget->GetActorRotation().Yaw + 15.f;
	if (bIsUpdate)
	{
		float DesiredYaw = BestRot.Yaw;

		// check if too far to change directly
		if (DesiredYaw - CurrentYaw > 180.f)
		{
			DesiredYaw -= 360.f;
		}
		else if (CurrentYaw - DesiredYaw > 180.f)
		{
			CurrentYaw -= 360.f;
		}
		if (DesiredYaw - CurrentYaw > 30.f)
		{
			BestRot.Yaw = CurrentYaw + 15.f + FMath::Clamp(60.f - (DesiredYaw - CurrentYaw), 0.f , 15.f);
		}
		else if (CurrentYaw - DesiredYaw > 30.f)
		{
			BestRot.Yaw = CurrentYaw - 15.f - FMath::Clamp(60.f - (CurrentYaw - DesiredYaw), 0.f, 15.f);
		}
	}
	float UnBlockedPct = (Cast<APawn>(TestViewTarget) && (PlayerCameraManager->FreeCamDistance > 0.f)) ? 96.f / PlayerCameraManager->FreeCamDistance : 1.f;

	AUTCTFFlag* Flag = Cast<AUTCTFFlag>(TestViewTarget);
	if (Flag)
	{
		if (Flag->IsHome() && Flag->HomeBase)
		{
			BestRot.Yaw = Flag->HomeBase->BestViewYaw;
		}
		else if (Flag->Holder && Flag->AttachmentReplication.AttachParent)
		{
			UnBlockedPct = (PlayerCameraManager->FreeCamDistance > 0.f) ? 96.f / PlayerCameraManager->FreeCamDistance : 1.f;
			BestRot.Yaw = Flag->AttachmentReplication.AttachParent->GetActorRotation().Yaw + 15.f;
		}
	}

	if ((TestViewTarget == FinalViewTarget) && Cast<AUTCharacter>(TestViewTarget))
	{
		UnBlockedPct = 1.f;
		BestRot.Yaw += 180.f;
	}

	// look for acceptable view
	float YawIncrement = 30.f;
	float OffsetMag = -60.f;
	float YawOffset = 0.f;
	bool bFoundGoodView = false;
	float BestView = BestRot.Yaw;
	float BestDist = 0.f;
	float StartYaw = BestRot.Yaw;
	int32 IncrementCount = 1;
	AUTPlayerCameraManager* CamMgr = Cast<AUTPlayerCameraManager>(PlayerCameraManager);
	while (!bFoundGoodView && (IncrementCount < 12) && CamMgr)
	{
		BestRot.Yaw = StartYaw + YawOffset;
		FVector TargetLoc = TestViewTarget->GetActorLocation();
		FVector Pos = TargetLoc + FRotationMatrix(BestRot).TransformVector(PlayerCameraManager->FreeCamOffset) - BestRot.Vector() * PlayerCameraManager->FreeCamDistance;
		FHitResult Result(1.f);
		CamMgr->CheckCameraSweep(Result, TestViewTarget, TargetLoc, Pos);
		float NewDist = (Result.Location - TargetLoc).SizeSquared();
		bFoundGoodView = Result.Time > UnBlockedPct;
		if (NewDist > BestDist)
		{
			BestDist = NewDist;
			BestView = BestRot.Yaw;
		}
		float NewOffset = (YawIncrement * IncrementCount);
		if ((IncrementCount % 2) == 0)
		{
			NewOffset *= -1.f;
		}
		IncrementCount++;
		YawOffset += NewOffset;
	}
	if (!bFoundGoodView)
	{
		BestRot.Yaw = BestView;
	}
	SetControlRotation(BestRot);
}

void AUTPlayerController::StartCameraControl()
{
	AUTPlayerCameraManager* CamMgr = Cast<AUTPlayerCameraManager>(PlayerCameraManager);
	if (CamMgr)
	{
		CamMgr->bAllowSpecCameraControl = true;
	}
}

void AUTPlayerController::EndCameraControl()
{
	AUTPlayerCameraManager* CamMgr = Cast<AUTPlayerCameraManager>(PlayerCameraManager);
	if (CamMgr)
	{
		CamMgr->bAllowSpecCameraControl = false;
	}
}

void AUTPlayerController::ViewSelf(FViewTargetTransitionParams TransitionParams)
{
	ServerViewSelf(TransitionParams);
}

void AUTPlayerController::ServerViewSelf_Implementation(FViewTargetTransitionParams TransitionParams)
{
	if (IsInState(NAME_Spectating))
	{
		if (GetSpectatorPawn() && (GetViewTarget() != GetSpectatorPawn()))
		{
			SetViewTarget(this, TransitionParams);
		}
		ClientViewSpectatorPawn(TransitionParams);
	}
}
void AUTPlayerController::ClientViewSpectatorPawn_Implementation(FViewTargetTransitionParams TransitionParams)
{
	if (GetViewTarget() != GetSpectatorPawn())
	{
		FVector CurrentViewLoc;
		FRotator CurrentViewRot;
		GetPlayerViewPoint(CurrentViewLoc, CurrentViewRot);
		AActor* NewViewTarget = (GetSpectatorPawn() != NULL) ? GetSpectatorPawn() : SpawnSpectatorPawn();
		if (NewViewTarget == NULL)
		{
			NewViewTarget = this;
		}
		// move spectator pawn to current view location
		NewViewTarget->SetActorLocationAndRotation(CurrentViewLoc, CurrentViewRot);
		ResetCameraMode();
		SetViewTarget(NewViewTarget, TransitionParams);
	}
}

void AUTPlayerController::ClientHalftime_Implementation()
{
	// Freeze all of the pawns
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (*It && !Cast<ASpectatorPawn>((*It).Get()))
		{
			(*It)->TurnOff();
		}
	}
}

void AUTPlayerController::TestResult(uint16 ButtonID)
{
}

void AUTPlayerController::Possess(APawn* PawnToPossess)
{
	Super::Possess(PawnToPossess);

	if (UTPlayerState->bHasHighScore)
	{
		AUTCharacter *UTChar = Cast<AUTCharacter>(GetPawn());
		if (UTChar != nullptr)
		{
			UTChar->bHasHighScore = true;
			UTChar->HasHighScoreChanged();
		}
	}

	AUTCharacter *UTChar = Cast<AUTCharacter>(GetPawn());
	if (UTChar != nullptr)
	{
		if (UTPlayerState->HatClass)
		{
			UTChar->HatVariant = UTPlayerState->HatVariant;
			UTChar->SetHatClass(UTPlayerState->HatClass);
		}

		if (UTPlayerState->EyewearClass)
		{
			UTChar->EyewearVariant = UTPlayerState->EyewearVariant;
			UTChar->SetEyewearClass(UTPlayerState->EyewearClass);
		}
	}
}

void AUTPlayerController::PawnLeavingGame()
{
	if (UTCharacter != NULL)
	{
		UTCharacter->PlayerSuicide();
	}
	// TODO: vehicles
	else
	{
		UnPossess();
	}
}

void AUTPlayerController::ServerBouncePing_Implementation(float TimeStamp)
{
	ClientReturnPing(TimeStamp);
}

bool AUTPlayerController::ServerBouncePing_Validate(float TimeStamp)
{
	return true;
}

void AUTPlayerController::ClientReturnPing_Implementation(float TimeStamp)
{
	AUTPlayerState* UTPS = Cast<AUTPlayerState>(PlayerState);
	if (UTPS)
	{
		UTPS->CalculatePing(GetWorld()->GetTimeSeconds()-TimeStamp);
	}
}

void AUTPlayerController::ServerUpdatePing_Implementation(float ExactPing)
{
	if (PlayerState)
	{
		PlayerState->ExactPing = ExactPing;
		PlayerState->Ping = FMath::Min(255, (int32)(ExactPing * 0.25f));
	}
}

bool AUTPlayerController::ServerUpdatePing_Validate(float ExactPing)
{
	return true;
}

void AUTPlayerController::PlayerTick( float DeltaTime )
{
	Super::PlayerTick(DeltaTime);
	if (StateName == FName(TEXT("GameOver")))
	{
		UpdateRotation(DeltaTime);
	}

	// if we have no UTCharacterMovement, we need to apply firing here since it won't happen from the component
	if (bDeferFireInputs && (GetPawn() == NULL || Cast<UUTCharacterMovement>(GetPawn()->GetMovementComponent()) == NULL))
	{
		ApplyDeferredFireInputs();
	}

	// Force ping update if servermoves aren't triggering it.
	if ((GetWorld()->GetTimeSeconds() - LastPingCalcTime > 0.5f) && (GetNetMode() == NM_Client))
	{
		LastPingCalcTime = GetWorld()->GetTimeSeconds();
		ServerBouncePing(GetWorld()->GetTimeSeconds());
	}

	if (PlayerState && PlayerState->bOnlySpectator && bAutoCam)
	{
		// possibly switch cameras
		ChooseBestCamera();
	}

	// Follow the last spectated player again when they respawn
	if ((StateName == NAME_Spectating) && LastSpectatedPlayerState && IsLocalController() && (!Cast<AUTProjectile>(GetViewTarget()) || GetViewTarget()->IsPendingKillPending()))
	{
		APawn* ViewTargetPawn = PlayerCameraManager->GetViewTargetPawn();
		AUTCharacter* ViewTargetCharacter = Cast<AUTCharacter>(ViewTargetPawn);
		if (!ViewTargetPawn || (ViewTargetCharacter && ViewTargetCharacter->IsDead()))
		{
			for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
			{
				APawn* Pawn = *Iterator;
				if (Pawn != nullptr && Pawn->PlayerState == LastSpectatedPlayerState)
				{
					ServerViewPawn(*Iterator);
				}
			}
		}
	}
	else if (PlayerState && PlayerState->bOnlySpectator && (GetViewTarget() == this))
	{
		ClientViewSpectatorPawn(FViewTargetTransitionParams());
	}

	if (bTacComView && PlayerState && PlayerState->bOnlySpectator)
	{
		UpdateTacComOverlays();
	}
}

void AUTPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if ((GetNetMode() == NM_DedicatedServer) && (CurrentlyViewedScorePS != NULL))
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS)
		{
			if ((GS->Teams.Num() > TeamStatsUpdateTeam) && (GS->Teams[TeamStatsUpdateTeam] != NULL))
			{
				if (TeamStatsUpdateIndex < GS->TeamStats.Num())
				{
					ClientUpdateTeamStats(TeamStatsUpdateTeam, GS->TeamStats[TeamStatsUpdateIndex], GS->Teams[TeamStatsUpdateTeam]->GetStatsValue(GS->TeamStats[TeamStatsUpdateIndex]));
				}
				TeamStatsUpdateIndex++;
				if (TeamStatsUpdateIndex >= GS->TeamStats.Num())
				{
					TeamStatsUpdateTeam++;
					TeamStatsUpdateIndex = 0;
				}
			}
			if (StatsUpdateIndex == 0)
			{
				LastScoreStatsUpdateStartTime = GetWorld()->GetTimeSeconds();
			}
			int32 StatArraySize = 0;
			FName StatsName = NAME_None;
			if (CurrentlyViewedStatsTab == 0)
			{
				StatArraySize = GS->GameScoreStats.Num();
				if (StatsUpdateIndex < StatArraySize)
				{
					StatsName = GS->GameScoreStats[StatsUpdateIndex];
				}
			}
			else if (CurrentlyViewedStatsTab == 1)
			{
				StatArraySize = GS->WeaponStats.Num();
				if (StatsUpdateIndex < StatArraySize)
				{
					StatsName = GS->WeaponStats[StatsUpdateIndex];
				}
			}
			else if (CurrentlyViewedStatsTab == 2)
			{
				StatArraySize = GS->RewardStats.Num();
				if (StatsUpdateIndex < StatArraySize)
				{
					StatsName = GS->RewardStats[StatsUpdateIndex];
				}
			}
			if (StatsUpdateIndex < StatArraySize)
			{
				ClientUpdateScoreStats(CurrentlyViewedScorePS, StatsName, CurrentlyViewedScorePS->GetStatsValue(StatsName));
			}
			StatsUpdateIndex++;
			if (StatsUpdateIndex >= StatArraySize)
			{
				if (LastScoreStatsUpdateStartTime < CurrentlyViewedScorePS->LastScoreStatsUpdateTime)
				{
					StatsUpdateIndex = 0;
				}
				else if (LastTeamStatsUpdateStartTime < GetWorld()->GetTimeSeconds() - 3.f)
				{
					TeamStatsUpdateTeam = 0;
					TeamStatsUpdateIndex = 0;
				}
			}
		}
	}
}

void AUTPlayerController::ChooseBestCamera()
{
	// for now, choose just between live players.  Eventually also use level cameras, etc.
	float BestScore = 0.f;
	APlayerState* BestPS = LastSpectatedPlayerState;
	AUTPlayerCameraManager* UTCam = Cast<AUTPlayerCameraManager>(PlayerCameraManager);
	if (UTCam)
	{
		for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
		{
			AUTCharacter* Pawn = Cast<AUTCharacter>(*Iterator);
			AUTPlayerState* NextPlayerState = (Pawn && (Pawn->Health > 0)) ? Cast<AUTPlayerState>(Pawn->PlayerState) : NULL;
			if (NextPlayerState)
			{
				float NewScore = UTCam->RatePlayerCamera(NextPlayerState, Pawn, LastSpectatedPlayerState);
				if (NewScore > BestScore)
				{
					BestScore = NewScore;
					BestPS = NextPlayerState;
				}
			}
		}
	}

	if (BestPS && (BestPS != LastSpectatedPlayerState))
	{
		ViewPlayerState(BestPS);
		BehindView(bSpectateBehindView);
	}
}

void AUTPlayerController::NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent)
{
	APlayerState* InstigatedByState = (InstigatedBy != NULL) ? InstigatedBy->PlayerState : NULL;
	FVector RelHitLocation(FVector::ZeroVector);
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		RelHitLocation = ((FPointDamageEvent*)&DamageEvent)->HitInfo.Location - GetViewTarget()->GetActorLocation();
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && ((FRadialDamageEvent*)&DamageEvent)->ComponentHits.Num() > 0)
	{
		RelHitLocation = ((FRadialDamageEvent*)&DamageEvent)->ComponentHits[0].Location - GetViewTarget()->GetActorLocation();
	}
	ClientNotifyTakeHit(InstigatedByState, Damage, Momentum, RelHitLocation, DamageEvent.DamageTypeClass);
}

void AUTPlayerController::ClientNotifyTakeHit_Implementation(APlayerState* InstigatedBy, int32 Damage, FVector Momentum, FVector RelHitLocation, TSubclassOf<UDamageType> DamageType)
{
	if (MyUTHUD != NULL)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		MyUTHUD->PawnDamaged(((GetPawn() != NULL) ? GetPawn()->GetActorLocation() : GetViewTarget()->GetActorLocation()) + RelHitLocation, Damage, DamageType, InstigatedBy != PlayerState && GS != NULL && GS->OnSameTeam(InstigatedBy, this));
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


void AUTPlayerController::K2_ReceiveLocalizedMessage(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

void AUTPlayerController::SwitchTeam()
{
	if (UTPlayerState && UTPlayerState->Team && (UTPlayerState->Team->TeamIndex < 2))
	{
		ChangeTeam(1 - UTPlayerState->Team->TeamIndex);
	}
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

void AUTPlayerController::SetWeaponHand(EWeaponHand NewHand)
{
	WeaponHand = NewHand;
	SaveConfig();
	if (!IsTemplate())
	{
		ServerSetWeaponHand(NewHand);
	}
}
bool AUTPlayerController::ServerSetWeaponHand_Validate(EWeaponHand NewHand)
{
	return true;
}
void AUTPlayerController::ServerSetWeaponHand_Implementation(EWeaponHand NewHand)
{
	WeaponHand = NewHand;
	AUTCharacter* UTCharTarget = Cast<AUTCharacter>(GetViewTarget());
	if (UTCharTarget != NULL && UTCharTarget->GetWeapon() != NULL)
	{
		UTCharTarget->GetWeapon()->UpdateWeaponHand();
	}
}

void AUTPlayerController::Emote(int32 EmoteIndex)
{
	if (GetWorld()->GetRealTimeSeconds() - LastEmoteTime > EmoteCooldownTime)
	{
		ServerEmote(EmoteIndex);
		LastEmoteTime = GetWorld()->GetRealTimeSeconds();
	}
}

void AUTPlayerController::PlayTaunt()
{
	Emote(0);
}

void AUTPlayerController::PlayTaunt2()
{
	Emote(1);
}

bool AUTPlayerController::ServerEmote_Validate(int32 EmoteIndex)
{
	return true;
}

void AUTPlayerController::ServerEmote_Implementation(int32 EmoteIndex)
{
	if (UTCharacter != nullptr && UTPlayerState != nullptr)
	{
		UTCharacter->PlayTauntByIndex(EmoteIndex);
	}
}

void AUTPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	
	if (LP != NULL)
	{
		if (GetNetMode() != NM_Standalone)
		{
			ServerSetWeaponHand(WeaponHand);
			if (FUTAnalytics::IsAvailable() && (GetWorld()->GetNetMode() != NM_Client || GetWorld()->GetNetDriver() != NULL)) // make sure we don't do analytics for demo playback
			{
				FString ServerInfo = (GetWorld()->GetNetMode() == NM_Client) ? GetWorld()->GetNetDriver()->ServerConnection->URL.ToString() : GEngine->GetWorldContextFromWorldChecked(GetWorld()).LastURL.ToString();
				FUTAnalytics::GetProvider().RecordEvent(TEXT("PlayerConnect"), TEXT("Server"), ServerInfo);
			}
		}

		IOnlineIdentityPtr OnlineIdentityInterface = (IOnlineSubsystem::Get() != NULL) ? IOnlineSubsystem::Get()->GetIdentityInterface() : NULL;
		if (OnlineIdentityInterface.IsValid() && OnlineIdentityInterface->GetLoginStatus(LP->GetControllerId()))
		{
			TSharedPtr<FUniqueNetId> UserId = OnlineIdentityInterface->GetUniquePlayerId(LP->GetControllerId());
			if (UserId.IsValid())
			{
				ServerReceiveStatsID(UserId->ToString());
			}
		}

		// Send over the country flag....
		UUTProfileSettings* Settings = LP->GetProfileSettings();
		if (Settings != NULL)
		{
			uint32 CountryFlag = Settings->CountryFlag;

			if (LP->CommunityRole != EUnrealRoles::Gamer)
			{
				// If we are a contributor ,but are trying to use the developer flag, set back to unreal flag
				if (LP->CommunityRole != EUnrealRoles::Developer)
				{
					if (CountryFlag == 142)
					{
						CountryFlag = 0;
					}
				}
			}
			else if (CountryFlag >= 140)
			{
				CountryFlag = 0;
			}		

			ServerReceiveCountryFlag(CountryFlag);
		}
	}
}

bool AUTPlayerController::ServerReceiveCountryFlag_Validate(uint32 NewCountryFlag)
{
	return true;
}
void AUTPlayerController::ServerReceiveCountryFlag_Implementation(uint32 NewCountryFlag)
{
	if (UTPlayerState != NULL)
	{
		UTPlayerState->CountryFlag = NewCountryFlag;

		if (FUTAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("CountryFlag"), UTPlayerState->CountryFlag));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("UserId"), UTPlayerState->UniqueId.ToString()));
			FUTAnalytics::GetProvider().RecordEvent(TEXT("FlagChange"), ParamArray);
		}
	}
}

bool AUTPlayerController::ServerReceiveStatsID_Validate(const FString& NewStatsID)
{
	return true;
}
/** Store an id for stats tracking.  Right now we are using the machine ID for this PC until we have have a proper ID available.  */
void AUTPlayerController::ServerReceiveStatsID_Implementation(const FString& NewStatsID)
{
	if (UTPlayerState != NULL && !GetWorld()->IsPlayInEditor() && GetWorld()->GetNetMode() != NM_Standalone)
	{
		UTPlayerState->StatsID = NewStatsID;
		UTPlayerState->ReadStatsFromCloud();
	}
}

void AUTPlayerController::ApplyDeferredFireInputs()
{
	for (FDeferredFireInput& Input : DeferredFireInputs)
	{
		if (Input.bStartFire)
		{
			if (UTCharacter != NULL)
			{
				if (StateName == NAME_Playing)
				{
					UTCharacter->StartFire(Input.FireMode);
				}
			}
			else if (GetPawn() != nullptr)
			{
				GetPawn()->PawnStartFire(Input.FireMode);
			}
		}
		else if (UTCharacter != NULL)
		{
			UTCharacter->StopFire(Input.FireMode);
		}
	}
	DeferredFireInputs.Empty();
}

bool AUTPlayerController::HasDeferredFireInputs()
{
	for (FDeferredFireInput& Input : DeferredFireInputs)
	{
		if (Input.bStartFire)
		{
			return true;
		}
	}
	return false;
}

void AUTPlayerController::SetEmoteSpeed(float NewEmoteSpeed)
{
	if (UTCharacter != nullptr)
	{
		UTCharacter->ServerSetEmoteSpeed(NewEmoteSpeed);
	}
}

void AUTPlayerController::FasterEmote()
{
	if (UTCharacter != nullptr && UTCharacter->EmoteCount > 0)
	{
		UTCharacter->ServerFasterEmote();
	}
}

void AUTPlayerController::SlowerEmote()
{
	if (UTCharacter != nullptr && UTCharacter->EmoteCount > 0)
	{
		UTCharacter->ServerSlowerEmote();
	}
}

bool AUTPlayerController::ServerViewPawn_Validate(APawn* PawnToView)
{
	return true;
}

void AUTPlayerController::ServerViewPawn_Implementation(APawn* PawnToView)
{
	// Don't view other pawns when we already have a pawn
	if (GetPawn() != nullptr)
	{
		return;
	}

	if (PawnToView)
	{
		SetViewTarget(PawnToView->PlayerState);
	}
}

void AUTPlayerController::SetMouseSensitivityUT(float NewSensitivity)
{
	PlayerInput->SetMouseSensitivity(NewSensitivity);

	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::MouseX || Entry.AxisKeyName == EKeys::MouseY)
		{
			Entry.AxisProperties.Sensitivity = NewSensitivity;
		}
	}

	InputSettings->SaveConfig();
}

float AUTPlayerController::GetWeaponAutoSwitchPriority(FString WeaponClassname, float DefaultPriority)
{
	if (Cast<UUTLocalPlayer>(Player))
	{
		UUTProfileSettings* ProfileSettings = Cast<UUTLocalPlayer>(Player)->GetProfileSettings();
		if (ProfileSettings)
		{
			return ProfileSettings->GetWeaponPriority(WeaponClassname, DefaultPriority);
		}
	}
	return DefaultPriority;
}


void AUTPlayerController::RconMap(FString NewMap)
{
	ServerRconMap(NewMap);
}

bool AUTPlayerController::ServerRconMap_Validate(const FString& NewMap)
{
	return true;
}

void AUTPlayerController::ServerRconMap_Implementation(const FString& NewMap)
{
	if (UTPlayerState != nullptr && UTPlayerState->bIsRconAdmin)
	{
		FString MapFullName;
		if (FPackageName::SearchForPackageOnDisk(NewMap + FPackageName::GetMapPackageExtension(), &MapFullName))
		{
			GetWorld()->ServerTravel(MapFullName, false);
		}
		else
		{
			ClientSay(UTPlayerState, FString::Printf(TEXT("Rcon %s doesn't exist"), *NewMap), ChatDestinations::System);
		}
	}
	else
	{
		ClientSay(UTPlayerState, TEXT("Rcon not authenticated"), ChatDestinations::System);
	}
}

void AUTPlayerController::RconNextMap(FString NextMap)
{
	ServerRconNextMap(NextMap);
}

bool AUTPlayerController::ServerRconNextMap_Validate(const FString& NextMap)
{
	return true;
}

void AUTPlayerController::ServerRconNextMap_Implementation(const FString& NextMap)
{
	if (UTPlayerState != nullptr && UTPlayerState->bIsRconAdmin)
	{
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (Game != nullptr)
		{
			FString MapFullName;
			if (FPackageName::SearchForPackageOnDisk(NextMap + FPackageName::GetMapPackageExtension(), &MapFullName))
			{
				Game->RconNextMapName = MapFullName;
				ClientSay(UTPlayerState, FString::Printf(TEXT("Next map set to %s"), *NextMap), ChatDestinations::System);
			}
			else
			{
				ClientSay(UTPlayerState, FString::Printf(TEXT("Rcon %s doesn't exist"), *NextMap), ChatDestinations::System);
			}
		}
	}
	else
	{
		ClientSay(UTPlayerState, TEXT("Rcon not authenticated"), ChatDestinations::System);
	}
}

void AUTPlayerController::BeginInactiveState()
{
	Super::BeginInactiveState();

	AGameState const* const GameState = GetWorld()->GameState;

	GetWorldTimerManager().SetTimer(SpectateKillerHandle, this, &AUTPlayerController::SpectateKiller, KillerSpectateDelay);
}

void AUTPlayerController::EndInactiveState()
{
	Super::EndInactiveState();

	GetWorldTimerManager().ClearTimer(SpectateKillerHandle);
}

void AUTPlayerController::SpectateKiller()
{
	/*
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != nullptr && GS->bViewKillerOnDeath && UTPlayerState->LastKillerPlayerState != nullptr && UTPlayerState->LastKillerPlayerState != UTPlayerState)
	{
		for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter *UTChar = Cast<AUTCharacter>(*It);
			if (UTChar != nullptr && UTChar->PlayerState == UTPlayerState->LastKillerPlayerState)
			{
				ServerViewPlaceholderAtLocation(UTChar->GetActorLocation() + FVector(0, 0, UTChar->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
			}
		}
	}
	*/
}

void AUTPlayerController::ServerViewPlaceholderAtLocation_Implementation(FVector Location)
{
	if (GetPawn() == NULL && (IsInState(NAME_Spectating) || IsInState(NAME_Inactive)))
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.bNoCollisionFail = true;
		AUTViewPlaceholder *ViewPlaceholder = GetWorld()->SpawnActor<AUTViewPlaceholder>(AUTViewPlaceholder::StaticClass(), Location, FRotator(), Params);
		SetViewTarget(ViewPlaceholder);
	}
}

bool AUTPlayerController::ServerViewPlaceholderAtLocation_Validate(FVector Location)
{
	return true;
}


void AUTPlayerController::UTBugIt(const FString& ScreenShotDescription)
{
	ConsoleCommand(FString::Printf(TEXT("BUGSCREENSHOTWITHHUDINFO %s"), *ScreenShotDescription));

	FVector ViewLocation;
	FRotator ViewRotation;
	GetPlayerViewPoint(ViewLocation, ViewRotation);

	if (GetPawn() != NULL)
	{
		ViewLocation = GetPawn()->GetActorLocation();
	}

	FString GoString, LocString;
	UTBugItStringCreator(ViewLocation, ViewRotation, GoString, LocString);

	UTLogOutBugItGoToLogFile(ScreenShotDescription, GoString, LocString);
}

void AUTPlayerController::UTBugItStringCreator(FVector ViewLocation, FRotator ViewRotation, FString& GoString, FString& LocString)
{
	GoString = FString::Printf(TEXT("BugItGo %f %f %f %f %f %f"), ViewLocation.X, ViewLocation.Y, ViewLocation.Z, ViewRotation.Pitch, ViewRotation.Yaw, ViewRotation.Roll);
	UE_LOG(LogUTPlayerController, Log, TEXT("%s"), *GoString);

	LocString = FString::Printf(TEXT("?BugLoc=%s?BugRot=%s"), *ViewLocation.ToString(), *ViewRotation.ToString());
	UE_LOG(LogUTPlayerController, Log, TEXT("%s"), *LocString);
}

void AUTPlayerController::UTLogOutBugItGoToLogFile(const FString& InScreenShotDesc, const FString& InGoString, const FString& InLocString)
{
#if ALLOW_DEBUG_FILES
	// Create folder if not already there

	const FString OutputDir = FPaths::BugItDir() + InScreenShotDesc + TEXT("/");

	IFileManager::Get().MakeDirectory(*OutputDir);
	// Create archive for log data.
	// we have to +1 on the GScreenshotBitmapIndex as it will be incremented by the bugitscreenshot which is processed next tick

	const FString DescPlusExtension = FString::Printf(TEXT("%s%i.txt"), *InScreenShotDesc, GScreenshotBitmapIndex);
	const FString TxtFileName = CreateProfileFilename(DescPlusExtension, false);

	//FString::Printf( TEXT("BugIt%s-%s%05i"), *GEngineVersion.ToString(), *InScreenShotDesc, GScreenshotBitmapIndex+1 ) + TEXT( ".txt" );
	const FString FullFileName = OutputDir + TxtFileName;

	FOutputDeviceFile OutputFile(*FullFileName);
	//FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter( *(FullFileName), FILEWRITE_Append );


	OutputFile.Logf(TEXT("Dumping BugIt data chart at %s using build %s built from changelist %i"), *FDateTime::Now().ToString(), *GEngineVersion.ToString(), GetChangeListNumberForPerfTesting());

	const FString MapNameStr = GetWorld()->GetMapName();

	OutputFile.Logf(TEXT("MapName: %s"), *MapNameStr);

	OutputFile.Logf(TEXT("Description: %s"), *InScreenShotDesc);
	OutputFile.Logf(TEXT("%s"), *InGoString);
	OutputFile.Logf(TEXT("%s"), *InLocString);
	
	// Flush, close and delete.
	//delete OutputFile;
	OutputFile.TearDown();

	// so here we want to send this bad boy back to the PC
	SendDataToPCViaUnrealConsole(TEXT("UE_PROFILER!BUGIT:"), *(FullFileName));
#endif // ALLOW_DEBUG_FILES
}

void AUTPlayerController::ClientSetLocation_Implementation(FVector NewLocation, FRotator NewRotation)
{
	Super::ClientSetLocation_Implementation(NewLocation, NewRotation);
	if (!GetPawn())
	{
		SetSpawnLocation(NewLocation);
	}
}

void AUTPlayerController::OnRep_CastingGuide()
{
	// we need the game state to work
	if (GetWorld()->GetGameState() == NULL)
	{
		FTimerHandle Useless;
		GetWorldTimerManager().SetTimer(Useless, this, &AUTPlayerController::OnRep_CastingGuide, 0.1f, false);
	}
	else if (GetWorld()->GetNetMode() == NM_Client && bCastingGuide && UTPlayerState != NULL)
	{
		ULocalPlayer* LP = Cast<ULocalPlayer>(Player);
		if (LP != NULL && LP->ViewportClient != NULL && LP->GetGameInstance() != NULL)
		{
			while (LP->GetGameInstance()->GetNumLocalPlayers() < LP->ViewportClient->MaxSplitscreenPlayers)
			{
				// partial copy from UGameInstance::CreateLocalPlayer() and ULocalPlayer::SendSplitJoin() as we want to do special join handling
				ULocalPlayer* NewPlayer = NewObject<ULocalPlayer>(GEngine, GEngine->LocalPlayerClass);
				int32 InsertIndex = LP->GetGameInstance()->AddLocalPlayer(NewPlayer, 255);
				if (InsertIndex == INDEX_NONE)
				{
					// something went wrong
					break;
				}
				else
				{
					FURL URL;
					URL.LoadURLConfig(TEXT("DefaultPlayer"), GGameIni);
					URL.AddOption(*FString::Printf(TEXT("Name=%s"), *FString::Printf(TEXT("%s_Spec%i"), *UTPlayerState->PlayerName, InsertIndex)));
					URL.AddOption(TEXT("CastingView=1"));
					URL.AddOption(TEXT("SpectatorOnly=1"));
					FString URLString = URL.ToString();
					FUniqueNetIdRepl UniqueId = LP->GetPreferredUniqueNetId();
					FNetControlMessage<NMT_JoinSplit>::Send(GetWorld()->GetNetDriver()->ServerConnection, URLString, UniqueId);
					NewPlayer->bSentSplitJoin = true;
				}
			}
		}
	}
}

void AUTPlayerController::OnRep_CastingViewIndex()
{
	if (CastingGuideStartupCommands.IsValidIndex(CastingGuideViewIndex) && !CastingGuideStartupCommands[CastingGuideViewIndex].IsEmpty())
	{
		ConsoleCommand(CastingGuideStartupCommands[CastingGuideViewIndex]);
	}
}

void AUTPlayerController::HUDSettings()
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP)
	{
		LP->ShowHUDSettings();	
	}
}
void AUTPlayerController::ResolveKeybindToFKey(FString Command, TArray<FKey>& Keys, bool bIncludeGamepad, bool bIncludeAxis)
{
	Keys.Empty();
	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	//Look though ActionMappings
	for (int32 i = 0; i < InputSettings->ActionMappings.Num(); i++)
	{
		if (InputSettings->ActionMappings[i].ActionName.ToString() == Command)
		{
			if (!InputSettings->ActionMappings[i].Key.IsGamepadKey() || bIncludeGamepad)
			{
				Keys.Add(InputSettings->ActionMappings[i].Key);
			}
		}
	}
	
	if (bIncludeAxis)
	{
		for (int32 i = 0; i < InputSettings->AxisMappings.Num(); i++)
		{
			if (InputSettings->AxisMappings[i].AxisName.ToString() == Command)
			{
				if (!InputSettings->AxisMappings[i].Key.IsGamepadKey() || bIncludeGamepad)
				{
					Keys.Add(InputSettings->AxisMappings[i].Key);
				}
			}
		}
	}

	// Look at my Custom Keybinds

	UUTPlayerInput* UTPlayerInput = Cast<UUTPlayerInput>(PlayerInput);
	if (UTPlayerInput)
	{
		for (int32 i = 0; i < UTPlayerInput->CustomBinds.Num(); i++)
		{
			if (UTPlayerInput->CustomBinds[i].Command == Command)
			{
				if (!FKey(UTPlayerInput->CustomBinds[i].KeyName).IsGamepadKey() || bIncludeGamepad)
				Keys.Add(FKey(UTPlayerInput->CustomBinds[i].KeyName));
			}
		}
		for (int32 i = 0; i < UTPlayerInput->SpectatorBinds.Num(); i++)
		{
			if (UTPlayerInput->SpectatorBinds[i].Command == Command)
			{
				if (!FKey(UTPlayerInput->SpectatorBinds[i].KeyName).IsGamepadKey() || bIncludeGamepad)
					Keys.Add(FKey(UTPlayerInput->SpectatorBinds[i].KeyName));
			}
		}
	}
}

void AUTPlayerController::ResolveKeybind(FString Command, TArray<FString>& Keys, bool bIncludeGamepad, bool bIncludeAxis)
{
	TArray<FKey> BoundKeys;
	ResolveKeybindToFKey(Command, BoundKeys, bIncludeGamepad, bIncludeAxis);

	for (int32 i=0;i<BoundKeys.Num(); i++)
	{
		Keys.Add(BoundKeys[i].ToString());
	}

}

void AUTPlayerController::DebugTest(FString TestCommand)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		LP->OpenLoadout(TestCommand.Equals(TEXT("buy"),ESearchCase::IgnoreCase));
	}
}

void AUTPlayerController::ClientRequireContentItemListComplete_Implementation()
{
	UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
	if (UTEngine)
	{
		if (UTEngine->FilesToDownload.Num() > 0)
		{
			// quit so we can download everything
			UTEngine->SetClientTravel(GetWorld(), TEXT("?downloadfiles"), TRAVEL_Relative);
		}
	}
}

int32 AUTPlayerController::ParseWeaponBind(FString ActionName)
{
	// Check if this is a switch weapon command, and if it 
	if (ActionName.Left(12).Equals(TEXT("switchweapon"), ESearchCase::IgnoreCase))
	{
		TArray<FString> Parsed;
		ActionName.ParseIntoArray(Parsed, TEXT(" "),true);
		if (Parsed.Num() == 2)
		{
			return FCString::Atoi(*Parsed[1]);
		}
	}
	return -1;
}

FString AUTPlayerController::FixedupKeyname(FString KeyName)
{
	if (KeyName.Equals(TEXT("one"), ESearchCase::IgnoreCase)) return TEXT("1");
	if (KeyName.Equals(TEXT("two"), ESearchCase::IgnoreCase)) return TEXT("2");
	if (KeyName.Equals(TEXT("three"), ESearchCase::IgnoreCase)) return TEXT("3");
	if (KeyName.Equals(TEXT("four"), ESearchCase::IgnoreCase)) return TEXT("4");
	if (KeyName.Equals(TEXT("five"), ESearchCase::IgnoreCase)) return TEXT("5");
	if (KeyName.Equals(TEXT("six"), ESearchCase::IgnoreCase)) return TEXT("6");
	if (KeyName.Equals(TEXT("seven"), ESearchCase::IgnoreCase)) return TEXT("7");
	if (KeyName.Equals(TEXT("eight"), ESearchCase::IgnoreCase)) return TEXT("8");
	if (KeyName.Equals(TEXT("nine"), ESearchCase::IgnoreCase)) return TEXT("9");
	if (KeyName.Equals(TEXT("zero"), ESearchCase::IgnoreCase)) return TEXT("0");

	return KeyName;
}

void AUTPlayerController::UpdateWeaponGroupKeys()
{
	WeaponGroupKeys.Empty();

	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();

	//Look though ActionMappings
	for (int32 i = 0; i < InputSettings->ActionMappings.Num(); i++)
	{
		int32 GroupIdx = ParseWeaponBind(InputSettings->ActionMappings[i].ActionName.ToString());
		if (GroupIdx >= 0)
		{
			if (!WeaponGroupKeys.Find(GroupIdx))
			{
				WeaponGroupKeys.Add(GroupIdx, FixedupKeyname(InputSettings->ActionMappings[i].Key.ToString()));
			}
		}
	}
	
	for (int32 i = 0; i < InputSettings->AxisMappings.Num(); i++)
	{
		int32 GroupIdx = ParseWeaponBind(InputSettings->AxisMappings[i].AxisName.ToString());
		if (GroupIdx >= 0)
		{
			if (!WeaponGroupKeys.Find(GroupIdx))
			{
				WeaponGroupKeys.Add(GroupIdx, FixedupKeyname(InputSettings->AxisMappings[i].AxisName.ToString()));
			}
		}
	}

	// Look at my Custom Keybinds

	UUTPlayerInput* UTPlayerInput = Cast<UUTPlayerInput>(PlayerInput);
	if (UTPlayerInput)
	{
		for (int32 i = 0; i < UTPlayerInput->CustomBinds.Num(); i++)
		{
			int32 GroupIdx = ParseWeaponBind(UTPlayerInput->CustomBinds[i].Command);
			if (GroupIdx >= 0)
			{
				if (!WeaponGroupKeys.Find(GroupIdx))
				{
					WeaponGroupKeys.Add(GroupIdx, FixedupKeyname(UTPlayerInput->CustomBinds[i].KeyName.ToString()));
				}
			}
		}
	}
}

bool AUTPlayerController::ServerRegisterBanVote_Validate(AUTPlayerState* BadGuy) { return true; }
void AUTPlayerController::ServerRegisterBanVote_Implementation(AUTPlayerState* BadGuy)
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState && UTPlayerState)
	{
		GameState->VoteForTempBan(BadGuy, UTPlayerState);	
	}
}

void AUTPlayerController::UpdateRotation(float DeltaTime)
{
	UUTPlayerInput* Input = Cast<UUTPlayerInput>(PlayerInput);
	if (Input)
	{
		if (Input->AccelerationPower > 0)
		{
			float BaseSensivity = Input->GetMouseSensitivity();
			FRotator UnscaledInput = RotationInput * (1.0f / BaseSensivity);
			float InputLength = FMath::Sqrt(UnscaledInput.Yaw * UnscaledInput.Yaw + UnscaledInput.Pitch * UnscaledInput.Pitch);
			float InputSpeed = InputLength / DeltaTime;
			if (InputSpeed > 0)
			{
				UE_LOG(LogUTPlayerController, Verbose, TEXT("AUTPlayerController::UpdateRotation Pre: %f %f Speed: %f"), RotationInput.Yaw, RotationInput.Pitch, InputSpeed);
				InputSpeed -= Input->AccelerationOffset;
				if (InputSpeed > 0)
				{
					float AdjustmentAmount = FMath::Pow(InputSpeed * Input->Acceleration, Input->AccelerationPower);
					if (Input->AccelerationMax > 0 && AdjustmentAmount > Input->AccelerationMax)
					{
						AdjustmentAmount = Input->AccelerationMax * DeltaTime;
					}

					// Scale rotation input by acceleration
					RotationInput = UnscaledInput * (BaseSensivity + AdjustmentAmount);
					UE_LOG(LogUTPlayerController, Verbose, TEXT("AUTPlayerController::UpdateRotation Post: %f %f"), RotationInput.Yaw, RotationInput.Pitch);
				}
			}
		}
	}

	Super::UpdateRotation(DeltaTime);
}

void AUTPlayerController::ClientOpenLoadout_Implementation(bool bBuyMenu)
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->OpenLoadout(bBuyMenu);
	}
}

void AUTPlayerController::ShowBuyMenu()
{
	// It's silly to send this to the server before handling it here.  I probably should just for safe keepeing but for now
	// just locally.

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && GS->AvailableLoadout.Num() > 0)
	{
		ClientOpenLoadout_Implementation(true);
	}
}

void AUTPlayerController::SetViewedScorePS(AUTPlayerState* NewViewedPS, uint8 NewStatsPage)
{
	if ((NewViewedPS != CurrentlyViewedScorePS) || (NewStatsPage != CurrentlyViewedStatsTab))
	{
		ServerSetViewedScorePS(NewViewedPS, NewStatsPage);
	}
	CurrentlyViewedScorePS = NewViewedPS;
	CurrentlyViewedStatsTab = NewStatsPage;
}

void AUTPlayerController::ServerSetViewedScorePS_Implementation(AUTPlayerState* NewViewedPS, uint8 NewStatsPage)
{
	if ((NewViewedPS != CurrentlyViewedScorePS) || (NewStatsPage != CurrentlyViewedStatsTab))
	{
		StatsUpdateIndex = 0;
		TeamStatsUpdateTeam = 0;
		TeamStatsUpdateIndex = 0;
	}
	CurrentlyViewedScorePS = NewViewedPS;
	CurrentlyViewedStatsTab = NewStatsPage;
}

bool AUTPlayerController::ServerSetViewedScorePS_Validate(AUTPlayerState* NewViewedPS, uint8 NewStatsPage)
{
	return true;
}

void AUTPlayerController::ClientUpdateScoreStats_Implementation(AUTPlayerState* ViewedPS, FName StatsName, float NewValue)
{
	if (ViewedPS)
	{
		ViewedPS->SetStatsValue(StatsName, NewValue);
	}
}

void AUTPlayerController::ClientUpdateTeamStats_Implementation(uint8 TeamNum, FName StatsName, float NewValue)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && (GS->Teams.Num() > TeamNum) && GS->Teams[TeamNum])
	{
		GS->Teams[TeamNum]->SetStatsValue(StatsName, NewValue);
	}
}
