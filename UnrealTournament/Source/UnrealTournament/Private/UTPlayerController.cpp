// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTGameInstance.h"
#include "UTCharacterMovement.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "UTPickupWeapon.h"
#include "UTAnnouncer.h"
#include "UTPlayerInput.h"
#include "UTPlayerCameraManager.h"
#include "UTCheatManager.h"
#include "UTCTFGameState.h"
#include "UTCTFGameMessage.h"
#include "UTPowerupUseMessage.h"
#include "Engine/Console.h"
#include "UTAnalytics.h"
#include "Online.h"
#include "UTOnlineGameSearchBase.h"
#include "OnlineSubsystemTypes.h"
#include "OnlineSubsystemUtils.h"
#include "UTDroppedPickup.h"
#include "UTGameEngine.h"
#include "UnrealNetwork.h"
#include "Engine/NetConnection.h"
#include "UTProfileSettings.h"
#include "UTProgressionStorage.h"
#include "UTViewPlaceholder.h"
#include "DataChannel.h"
#include "Engine/GameInstance.h"
#include "UTSpectatorCamera.h"
#include "UTHUDWidget_NetInfo.h"
#include "UTWorldSettings.h"
#include "Engine/DemoNetDriver.h"
#include "UTGhostComponent.h"
#include "UTGameEngine.h"
#include "UTFlagInfo.h"
#include "UTProfileItem.h"
#include "UTMutator.h"
#include "UTVictimMessage.h"
#include "SUTSpawnWindow.h"
#include "IAnalyticsProvider.h"
#include "UTPlaceablePowerup.h"
#include "UTKillcamPlayback.h"
#include "UTWeaponAttachment.h"
#include "UTGameViewportClient.h"
#include "UTHeartBeatManager.h"
#include "QosInterface.h"
#include "UTGameObjective.h"
#include "UTFlagRunGameState.h"
#include "UTRemoteRedeemer.h"
#include "UTKillerTarget.h"
#include "UTGauntletFlag.h"
#include "UTTutorialAnnouncement.h"
#include "UTLineUpHelper.h"
#include "UTRallyPoint.h"
#include "UTDemoRecSpectator.h"

static TAutoConsoleVariable<float> CVarUTKillcamStartDelay(
	TEXT("UT.KillcamStartDelay"),
	0.5f,
	TEXT("Number of seconds to wait after dying to play the killcam.")
	);

DEFINE_LOG_CATEGORY_STATIC(LogUTPlayerController, Log, All);

AUTPlayerController::AUTPlayerController(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	MaxDodgeClickTime = 0.25f;
	MaxDodgeTapTime = 0.3f;
	LastTapLeftTime = -10.f;
	LastTapRightTime = -10.f;
	LastTapForwardTime = -10.f;
	LastTapBackTime = -10.f;
	bCrouchTriggersSlide = true;
	bSingleTapWallDodge = true;
	bSingleTapAfterJump = true;
	bAutoCam = true;

	bHearsTaunts = true;
	LastSameTeamTime = 0.f;

	PlayerCameraManagerClass = AUTPlayerCameraManager::StaticClass();
	CheatClass = UUTCheatManager::StaticClass();

	WeaponBobGlobalScaling = 1.f;
	EyeOffsetGlobalScaling = 1.f;

	ConfigDefaultFOV = 100.0f;
	FFAPlayerColor = FLinearColor(0.020845f, 0.335f, 0.0f, 1.0f);

	LastEmoteTime = 0.0f;
	EmoteCooldownTime = 0.3f;
	bSpectateBehindView = true;
	StylizedPPIndex = INDEX_NONE;

	PredictionFudgeFactor = 20.f;
	MaxPredictionPing = 0.f; 
	DesiredPredictionPing = 120.f;
	bIsDebuggingProjectiles = false;
	CastingGuideViewIndex = INDEX_NONE;
	bRequestingSlideOut = true;
	DilationIndex = 2;
	bNeedsRallyNotify = true;
	bNeedsBoostNotify = true;

	TimeToHoldPowerUpButtonToActivate = 0.75f;
	ScoreboardDelayOnDeath = 2.f;

	static ConstructorHelpers::FObjectFinder<USoundBase> PressedSelect(TEXT("SoundCue'/Game/RestrictedAssets/UI/UT99UI_LittleSelect_Cue.UT99UI_LittleSelect_Cue'"));
	SelectSound = PressedSelect.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> ChatMsgSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/UI/A_UI_Chat01.A_UI_Chat01'"));
	ChatMsgSound = ChatMsgSoundFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> BadSelect(TEXT("SoundCue'/Game/RestrictedAssets/UI/BadSelect_Cue.BadSelect_Cue'"));
	BadSelectSound = BadSelect.Object;

	static ConstructorHelpers::FObjectFinder<USoundBase> BoostActivateSoundFinder(TEXT("SoundWave'/Game/RestrictedAssets/Audio/Stingers/BoostActivated.BoostActivated'"));
	BoostActivateSound = BoostActivateSoundFinder.Object;

	LastBuyMenuOpenTime = 0.0f;
	BuyMenuToggleDelay = 0.25f;

	FootStepAmp.OwnVolumeMultiplier = 0.35f;
	FootStepAmp.TeammateVolumeMultiplier = 0.5f;
	PainSoundAmp.InstigatorVolumeMultiplier = 2.5f;
	PainSoundAmp.TargetVolumeMultiplier = 2.5f;
	WeaponFireAmp.InstigatorVolumeMultiplier = 1.3f;
	WeaponFireAmp.TargetVolumeMultiplier = 1.2f;
	WeaponFireAmp.TargetPitchMultiplier = 1.1f;

	static ConstructorHelpers::FObjectFinder<USoundAttenuation> InstigatedPainAttenFinder(TEXT("SoundAttenuation'/Game/RestrictedAssets/Audio/SoundClassesAndMixes/Attenuations/Attenuation_InstigatedPain.Attenuation_InstigatedPain'"));
	PainSoundAmp.TargetAttenuation = InstigatedPainAttenFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundAttenuation> InstigatedWeaponAttenFinder(TEXT("SoundAttenuation'/Game/RestrictedAssets/Audio/SoundClassesAndMixes/Attenuations/Attenuation_WeaponInstigator.Attenuation_WeaponInstigator'"));
	WeaponFireAmp.InstigatorAttenuation = InstigatedWeaponAttenFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundAttenuation> TargetWeaponAttenFinder(TEXT("SoundAttenuation'/Game/RestrictedAssets/Audio/SoundClassesAndMixes/Attenuations/Attenuation_WeaponTarget.Attenuation_WeaponTarget'"));
	WeaponFireAmp.TargetAttenuation = TargetWeaponAttenFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundAttenuation> OccludedWeaponAttenFinder(TEXT("SoundAttenuation'/Game/RestrictedAssets/Audio/SoundClassesAndMixes/Attenuations/Attenuation_WeaponFireOccluded.Attenuation_WeaponFireOccluded'"));
	WeaponFireAmp.OccludedAttenuation = OccludedWeaponAttenFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundAttenuation> InstigatedFoleyAttenFinder(TEXT("SoundAttenuation'/Game/RestrictedAssets/Audio/SoundClassesAndMixes/Attenuations/Attenuation_ProjectileFoleyInstigator.Attenuation_ProjectileFoleyInstigator'"));
	WeaponFoleyAmp.InstigatorAttenuation = InstigatedFoleyAttenFinder.Object;

	static ConstructorHelpers::FObjectFinder<USoundClass> SoundClassFinder(TEXT("SoundClass'/Game/RestrictedAssets/Audio/SoundClassesAndMixes/SFX_Priority.SFX_Priority'"));
	PriorityFXSoundClass = SoundClassFinder.Object;

	LastComMessageSwitch = -1;
	LastComMessageTime = 0.0f;
	ReplicatedWeaponHand = EWeaponHand::HAND_Right;
	MinNetUpdateFrequency = 100.0f;
}

void AUTPlayerController::BeginPlay()
{
	bSpectatorMouseChangesView = false;
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
	DOREPLIFETIME_CONDITION(AUTPlayerController, HUDClass, COND_OwnerOnly);
}
void AUTPlayerController::ClientSetSpectatorLocation_Implementation(FVector NewLocation, FRotator NewRotation)
{
	if (GetSpectatorPawn())
	{
		GetSpectatorPawn()->TeleportTo(NewLocation, NewRotation, false, true);
		SetControlRotation(NewRotation);
	}
}

void AUTPlayerController::SendPersonalMessage(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	if (GetPawn())
	{
		// send to spectators viewing this pawn as well;
		for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(*Iterator);
			if (PC && PC->UTPlayerState && PC->UTPlayerState->bOnlySpectator && ((PC->GetViewTarget() == GetPawn()) || PC->UTPlayerState->bIsDemoRecording))
			{
				PC->ClientReceivePersonalMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
			}
		}
	}
}

void AUTPlayerController::ClientReceivePersonalMessage_Implementation(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	// only pass on if viewing one of the playerstates
	APlayerState* ViewTargetPS = Cast<APawn>(GetViewTarget()) ? ((APawn *)(GetViewTarget()))->PlayerState : NULL;
	bool bViewingTarget = (ViewTargetPS == RelatedPlayerState_1) || (ViewTargetPS == RelatedPlayerState_2);
	if (!bViewingTarget && !Cast<ASpectatorPawn>(GetViewTarget()))
	{
		bViewingTarget = (Cast<AUTPlayerState>(RelatedPlayerState_1) && (((AUTPlayerState*)(RelatedPlayerState_1))->SpectatingID == LastSpectatedPlayerId))
			|| (Cast<AUTPlayerState>(RelatedPlayerState_2) && (((AUTPlayerState*)(RelatedPlayerState_2))->SpectatingID == LastSpectatedPlayerId));
	}
	if (bViewingTarget)
	{
		ClientReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	}
}

void AUTPlayerController::NetStats()
{
	bShowNetInfo = !bShowNetInfo;
	UNetDriver* NetDriver = GetWorld()->GetNetDriver();
	if (NetDriver)
	{
		NetDriver->bCollectNetStats = bShowNetInfo;
	}
	if (MyUTHUD && !MyUTHUD->HasHudWidget(UUTHUDWidget_NetInfo::StaticClass()))
	{
		NetInfoWidget = Cast<UUTHUDWidget_NetInfo>(MyUTHUD->AddHudWidget(UUTHUDWidget_NetInfo::StaticClass()));
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

void AUTPlayerController::Mutate(FString MutateString)
{
	ServerMutate(MutateString);
}

bool AUTPlayerController::ServerMutate_Validate(const FString& MutateString)
{
	return true;
}

void AUTPlayerController::ServerMutate_Implementation(const FString& MutateString)
{
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode != NULL && GameMode->BaseMutator != NULL)
	{
		GameMode->BaseMutator->Mutate(MutateString, this);
	}
}

void AUTPlayerController::ClientStartRally_Implementation(AUTRallyPoint* RallyTarget, const FVector& NewRallyLocation, float Delay)
{
	if (RallyTarget)
	{
		// client side
		if (UTPlayerState != nullptr)
		{
			UTPlayerState->RallyLocation = NewRallyLocation;
		}
		EndRallyTime = GetWorld()->GetTimeSeconds() + Delay;
		static FName NAME_RallyCam(TEXT("RallyCam"));
		SetCameraMode(NAME_RallyCam);
		SetViewTarget(RallyTarget);
		FRotator NewRotation = RallyTarget->GetActorRotation();
		NewRotation.Pitch = 0.f;
		NewRotation.Roll = 0.f;
		SetControlRotation(NewRotation);
	}
}

void AUTPlayerController::ClientCompleteRally_Implementation()
{
	if (GetPawn())
	{
		//client side
		BehindView(bPlayBehindView);
		SetViewTarget(GetPawn());
	}
}

void AUTPlayerController::RequestRally()
{
	if (UTPlayerState && UTPlayerState->bCanRally)
	{
		ServerRequestRally();
	}
}

bool AUTPlayerController::ServerRequestRally_Validate()
{
	return true;
}

void AUTPlayerController::ServerRequestRally_Implementation()
{
	if (UTCharacter && !UTCharacter->IsRagdoll())
	{
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (Game)
		{
			Game->HandleRallyRequest(this);
		}
	}
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
	if (Player)
	{
#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!UUTGameEngine::StaticClass()->GetDefaultObject<UUTGameEngine>()->bAllowClientNetProfile)
		{
			return;
		}
#endif
		Player->Exec(GetWorld(), *FString::Printf(TEXT("NETPROFILE")), *GLog);
	}
}

bool AUTPlayerController::ServerNotifyProjectileHit_Validate(AUTProjectile* HitProj, FVector_NetQuantize HitLocation, AActor* DamageCauser, float TimeStamp, int32 Damage)
{
	return true;
}

void AUTPlayerController::ServerNotifyProjectileHit_Implementation(AUTProjectile* HitProj, FVector_NetQuantize HitLocation, AActor* DamageCauser, float TimeStamp, int32 Damage)
{
	// @TODO FIXMESTEVE - need to verify shot from player's location at timestamp to HitLocation is valid, and that projectile should have been there at that time
	if (HitProj)
	{
		HitProj->NotifyClientSideHit(this, HitLocation, DamageCauser, Damage);
	}
}

void AUTPlayerController::ToggleSingleTap()
{
	bSingleTapWallDodge = !bSingleTapWallDodge;
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
	InputComponent->BindAction("Jump", IE_Released, this, &AUTPlayerController::JumpRelease);
	InputComponent->BindAction("Crouch", IE_Pressed, this, &AUTPlayerController::Crouch);
	InputComponent->BindAction("Crouch", IE_Released, this, &AUTPlayerController::UnCrouch);
	InputComponent->BindAction("ToggleCrouch", IE_Pressed, this, &AUTPlayerController::ToggleCrouch);
	InputComponent->BindAction("Slide", IE_Pressed, this, &AUTPlayerController::Slide);
	InputComponent->BindAction("Slide", IE_Released, this, &AUTPlayerController::StopSlide);

	InputComponent->BindAction("TapLeft", IE_Pressed, this, &AUTPlayerController::OnTapLeft);
	InputComponent->BindAction("TapRight", IE_Pressed, this, &AUTPlayerController::OnTapRight);
	InputComponent->BindAction("TapForward", IE_Pressed, this, &AUTPlayerController::OnTapForward);
	InputComponent->BindAction("TapBack", IE_Pressed, this, &AUTPlayerController::OnTapBack);
	InputComponent->BindAction("SingleTapDodge", IE_Pressed, this, &AUTPlayerController::OnSingleTapDodge);
	InputComponent->BindAction("HoldDodge", IE_Pressed, this, &AUTPlayerController::HoldDodge);
	InputComponent->BindAction("HoldDodge", IE_Released, this, &AUTPlayerController::ReleaseDodge);

	InputComponent->BindAction("TapLeft", IE_Released, this, &AUTPlayerController::OnTapLeftRelease);
	InputComponent->BindAction("TapRight", IE_Released, this, &AUTPlayerController::OnTapRightRelease);
	InputComponent->BindAction("TapForward", IE_Released, this, &AUTPlayerController::OnTapForwardRelease);
	InputComponent->BindAction("TapBack", IE_Released, this, &AUTPlayerController::OnTapBackRelease);

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
	InputComponent->BindAction("PlayGroupTaunt", IE_Pressed, this, &AUTPlayerController::PlayGroupTaunt);

	InputComponent->BindAction("ShowBuyMenu", IE_Pressed, this, &AUTPlayerController::ShowBuyMenu);

	InputComponent->BindAction("DropCarriedObject", IE_Pressed, this, &AUTPlayerController::DropCarriedObject);

	InputComponent->BindAction("ToggleComMenu", IE_Pressed, this, &AUTPlayerController::ShowComsMenu);
	InputComponent->BindAction("ToggleComMenu", IE_Released, this, &AUTPlayerController::HideComsMenu);

	InputComponent->BindAction("ToggleWeaponWheel", IE_Pressed, this, &AUTPlayerController::ShowWeaponWheel);
	InputComponent->BindAction("ToggleWeaponWheel", IE_Released, this, &AUTPlayerController::HideWeaponWheel);

	InputComponent->BindAction("PushToTalk", IE_Pressed, this, &AUTPlayerController::StartVOIPTalking);
	InputComponent->BindAction("PushToTalk", IE_Released, this, &AUTPlayerController::StopVOIPTalking);

	InputComponent->BindAction("RequestRally", IE_Pressed, this, &AUTPlayerController::RequestRally);

	UpdateWeaponGroupKeys();
	UpdateInventoryKeys();
}

void AUTPlayerController::ProcessPlayerInput(const float DeltaTime, const bool bGamePaused)
{
	if (InputEnabled())
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
}

void AUTPlayerController::InitInputSystem()
{
	if (PlayerInput == NULL)
	{
		PlayerInput = NewObject<UUTPlayerInput>(this, UUTPlayerInput::StaticClass());
	}

	Super::InitInputSystem();

	if (AnnouncerPath.ToString().Len() > 0)
	{
		TSubclassOf<UUTAnnouncer> AnnouncerClass = LoadClass<UUTAnnouncer>(NULL, *AnnouncerPath.ToString(), NULL, 0, NULL);
		if (AnnouncerClass != NULL)
		{
			Announcer = NewObject<UUTAnnouncer>(this, AnnouncerClass);
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

void AUTPlayerController::SetPlayer(UPlayer* InPlayer)
{
	Super::SetPlayer(InPlayer);

	ULocalPlayer* LP = Cast<ULocalPlayer>(Player);
	if (LP)
	{
		UUTGameViewportClient* VC = Cast<UUTGameViewportClient>(LP->ViewportClient);
		if (VC)
		{
			VC->SetActiveLocalPlayerControllers();
		}
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
			UTCharacter->UTCharacterMovement->UpdateFloorSlide(bIsHoldingFloorSlide);
			bNeedsRallyNotify = true;
			bNeedsBoostNotify = true;
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

	if (UTPlayerState != nullptr)
	{
		UTPlayerState->RespawnChoiceA = nullptr;
		UTPlayerState->RespawnChoiceB = nullptr;
	}

	SetCameraMode("Default");
	DeathCamFocus = nullptr;

	//There is an out of order chance during the initial connection that:
	// The new players character will be spawned and possessed. Replicating the characters PlayerState.
	// The GameStates spectator class will be replicated. Since (state == NAME_Spectating), BeginSpectatingState() will cause the Character to be unpossessed, setting PlayerState = NULL locally
	// This function will be called, setting the pawn back to the Character, leaving Characters PlayerState still NULL

	// Probably not the best solution. Make sure the PlayerState is set
	if (Role < ROLE_Authority && GetPawn() != nullptr && GetPawn()->PlayerState == nullptr)
	{
		GetPawn()->PlayerState = PlayerState;
	}

	// JackP recommended course of action
	static auto DitheredLODCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("foliage.DitheredLOD"));
	DitheredLODCVar->Set(false, ECVF_SetByGameSetting);
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AUTPlayerController::ResetFoliageDitheredLOD, 0.5f, false);

	if (MyUTHUD != nullptr) 
	{
		MyUTHUD->ClientRestart();
	}
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
			bool bAllowAnyFOV = false;
			if (GetNetMode() == NM_Standalone)
			{
				AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
				bAllowAnyFOV = (Game && !Game->bOfflineChallenge && !Game->bBasicTrainingGame);
			}
			PlayerCameraManager->DefaultFOV = bAllowAnyFOV ? NewFOV : ConfigDefaultFOV;
		}
		if (GetPawn() != NULL && GetNetMode() != NM_Standalone)
		{
			Suicide();
		}
		SaveConfig();
	}
}

void AUTPlayerController::SetSpectatorMouseChangesView(bool bNewValue)
{
	if (bSpectatorMouseChangesView != bNewValue)
	{
		bSpectatorMouseChangesView = bNewValue;
		UE_LOG(UT,Log, TEXT("---- bSpectatorMouseChangesView = %s"), (bSpectatorMouseChangesView ? TEXT("true") : TEXT("false")));

		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
		UUTGameInstance* UTGameInstance = LocalPlayer ? Cast<UUTGameInstance>(LocalPlayer->GetGameInstance()) : nullptr;

		if (bSpectatorMouseChangesView)
		{
			if (LocalPlayer && UTGameInstance && !UTGameInstance->bLevelIsLoading)
			{
				FReply& SlateOps = LocalPlayer->GetSlateOperations();
				SlateOps.UseHighPrecisionMouseMovement(LocalPlayer->ViewportClient->GetGameViewportWidget().ToSharedRef());
				SavedMouseCursorLocation = FSlateApplication::Get().GetCursorPos();
				bShowMouseCursor = false;
			}
		}
		else
		{
			if (LocalPlayer && UTGameInstance && !UTGameInstance->bLevelIsLoading)
			{
				LocalPlayer->GetSlateOperations().ReleaseMouseCapture().SetMousePos(SavedMouseCursorLocation.IntPoint());
				bShowMouseCursor = true;
			}
		}
	}
}

bool AUTPlayerController::InputAxis(FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (MyUTHUD != nullptr) 
	{
		if ( MyUTHUD->ProcessInputAxis(Key, Delta) )
		{
			return true;
		}
	}

	Super::InputAxis(Key, Delta, DeltaTime, NumSamples, bGamepad);

	return false;
}

bool AUTPlayerController::InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	// HACK: Ignore all input that occurred during loading to avoid Slate focus issues and other weird behaviour
	if (GetWorld()->RealTimeSeconds < 0.5f)
	{
		return true;
	}

	if ( MyUTHUD != nullptr && MyUTHUD->ProcessInputKey(Key, EventType) )
	{
		return true;
	}

	// pass mouse events to HUD if requested
	if (bShowMouseCursor && MyUTHUD != NULL && Key.IsMouseButton() && MyUTHUD->OverrideMouseClick(Key, EventType))
	{
		return true;
	}

#if !UE_SERVER
	else if (UTPlayerState && (UTPlayerState->bOnlySpectator || UTPlayerState->bOutOfLives) && (Key == EKeys::LeftMouseButton || Key == EKeys::RightMouseButton) && EventType == IE_Released && bSpectatorMouseChangesView)
	{
		SetSpectatorMouseChangesView(false);
	}
#endif

	//This is a separate from OnFire() since we dont want casters starting games by accident when clicking the mouse while flying around
	static FName NAME_Enter(TEXT("Enter"));
	if (Key.GetFName() == NAME_Enter && (EventType == IE_Pressed) && UTPlayerState != nullptr)
	{
		AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
		if (UTPlayerState->bCaster && !UTPlayerState->bReadyToPlay)
		{
			ServerRestartPlayer();
			return true;
		}
		else if (UTGameState && (UTGameState->GetMatchState() == MatchState::WaitingToStart) && UTPlayerState->bReadyToPlay && !UTPlayerState->bOnlySpectator)
		{
			ServerToggleWarmup();
			bPlayerIsWaiting = true;
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
				float TestPriority = GetWeaponAutoSwitchPriority(*It);
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

void AUTPlayerController::ClientGotWeaponStayPickup_Implementation(AUTPickupWeapon* Pickup, APawn* TouchedBy)
{
	if (Pickup != nullptr && TouchedBy != nullptr && TouchedBy == GetPawn())
	{
		Pickup->LocalPickupHandling(TouchedBy);
	}
}

bool AUTPlayerController::ServerActivatePowerUpPress_Validate()
{
	return true;
}

void AUTPlayerController::ServerActivatePowerUpPress_Implementation()
{
	if (UTCharacter != NULL && UTPlayerState != NULL && !GetWorldTimerManager().IsTimerActive(TriggerBoostTimerHandle))
	{
		AUTGameMode* UTGM = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (UTGM && UTGM->TriggerBoost(this))
		{
			UTClientPlaySound(BoostActivateSound);
			GetWorldTimerManager().SetTimer(TriggerBoostTimerHandle, this, &AUTPlayerController::TriggerBoost, TimeToHoldPowerUpButtonToActivate, false);
			// spawn effect
			TSubclassOf<AUTInventory> ActivatedPowerupPlaceholderClass = UTGM ? UTGM->GetActivatedPowerupPlaceholderClass() : nullptr;
			if (ActivatedPowerupPlaceholderClass)
			{
				UTCharacter->AddInventory(GetWorld()->SpawnActor<AUTInventory>(ActivatedPowerupPlaceholderClass, FVector(0.0f), FRotator(0.0f, 0.0f, 0.0f)), true);
			}
		}
	}
}

void AUTPlayerController::TriggerBoost()
{
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode && UTCharacter && UTPlayerState)
	{
		if (GameMode->AttemptBoost(this))
		{
			if (UTPlayerState->BoostClass)
			{
				AUTInventory* Powerup = UTPlayerState->BoostClass->GetDefaultObject<AUTInventory>();
				if (Powerup && Powerup->bNotifyTeamOnPowerupUse && GameMode->UTGameState && UTPlayerState->Team)
				{
					TeamNotifiyOfPowerupUse();
				}

				AUTPlaceablePowerup* FoundPlaceablePowerup = UTCharacter->FindInventoryType<AUTPlaceablePowerup>(AUTPlaceablePowerup::StaticClass(), false);
				if (FoundPlaceablePowerup)
				{
					FoundPlaceablePowerup->SpawnPowerup();
				}
				else if (!UTPlayerState->BoostClass.GetDefaultObject()->HandleGivenTo(UTCharacter))
				{
					AUTInventory* TriggeredBoost = GetWorld()->SpawnActor<AUTInventory>(UTPlayerState->BoostClass, FVector(0.0f), FRotator(0.f, 0.f, 0.f));
					TriggeredBoost->InitAsTriggeredBoost(UTCharacter);

					AUTInventory* DuplicatePowerup = UTCharacter->FindInventoryType<AUTInventory>(UTPlayerState->BoostClass, true);
					if (!DuplicatePowerup || !DuplicatePowerup->StackPickup(nullptr))
					{
						UTCharacter->AddInventory(TriggeredBoost, true);
					}

					//if we gave you a weapon lets immediately switch on triggering the boost
					AUTWeapon* BoostAsWeapon = Cast<AUTWeapon>(TriggeredBoost);
					if (BoostAsWeapon)
					{
						UTCharacter->SwitchWeapon(BoostAsWeapon);
					}
				}
			}
		}
	}
}


void AUTPlayerController::TeamNotifiyOfPowerupUse()
{
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode && UTPlayerState)
	{
		if (UTPlayerState->BoostClass)
		{
			AUTInventory* Powerup = UTPlayerState->BoostClass->GetDefaultObject<AUTInventory>();
			if (Powerup && GameMode->UTGameState && UTPlayerState->Team)
			{
				for (int32 PlayerIndex = 0; PlayerIndex < GameMode->UTGameState->PlayerArray.Num(); ++PlayerIndex)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(GameMode->UTGameState->PlayerArray[PlayerIndex]);
					if (PS && PS->Team)
					{
						if (PS->Team->TeamIndex == UTPlayerState->Team->TeamIndex)
						{
							AUTPlayerController* PC = Cast<AUTPlayerController>(PS->GetOwner());
							if (PC)
							{
								if (Powerup->bNotifyTeamOnPowerupUse)
								{
									//21 is Powerup Message
									PC->ClientReceiveLocalizedMessage(UUTPowerupUseMessage::StaticClass(), 21, UTPlayerState);
								}
							}
						}
					}
				}
			}
		}
	}
}

void AUTPlayerController::ActivateSpecial()
{
	if (UTPlayerState && UTPlayerState->BoostClass)
	{
		ServerActivatePowerUpPress();
	}
	else
	{
		ToggleTranslocator();
	}
}

void AUTPlayerController::ToggleTranslocator()
{
	if (UTCharacter != NULL && UTCharacter->GetWeapon() != NULL && IsLocalPlayerController())
	{
		int32 CurrentGroup = GetWeaponGroup(UTCharacter->GetWeapon());
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

void AUTPlayerController::SelectTranslocator()
{
	if (UTCharacter != NULL && UTCharacter->GetWeapon() != NULL && IsLocalPlayerController())
	{
		int32 CurrentGroup = GetWeaponGroup(UTCharacter->GetWeapon());
		if (CurrentGroup != 0)
		{
			ToggleTranslocator();
		}
	}
}

void AUTPlayerController::ThrowWeapon()
{
	if (UTCharacter != NULL && IsLocalPlayerController() && !UTCharacter->IsFiringDisabled())
	{
		if (UTCharacter->GetWeapon() != nullptr && UTCharacter->GetWeapon()->DroppedPickupClass != nullptr && UTCharacter->GetWeapon()->bCanThrowWeapon)
		{
			ServerThrowWeapon();
		}
	}
	else if ((UTCharacter == nullptr) && UTPlayerState && (UTPlayerState->ReadyMode > 1))
	{
		ServerThrowWeapon();
	}
}

bool AUTPlayerController::ServerThrowWeapon_Validate()
{
	return true;
}

void AUTPlayerController::ServerThrowWeapon_Implementation()
{
	if (UTCharacter != nullptr && !UTCharacter->IsFiringDisabled())
	{
		AUTGameMode* UTGM = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (UTGM && !UTGM->bBasicTrainingGame && UTCharacter->GetWeapon() != nullptr && UTCharacter->GetWeapon()->DroppedPickupClass != nullptr && UTCharacter->GetWeapon()->bCanThrowWeapon && !UTCharacter->GetWeapon()->IsFiring())
		{
			UTCharacter->TossInventory(UTCharacter->GetWeapon(), FVector(400.0f, 0, 200.f));
		}
	}
	else if ((UTCharacter == nullptr) && UTPlayerState && (UTPlayerState->ReadyMode > 1))
	{
		UTPlayerState->ReadyMode = 4;
	}
}

void AUTPlayerController::SwitchWeaponInSequence(bool bPrev)
{
	if (UTCharacter != NULL && IsLocalPlayerController() && UTCharacter->TauntCount == 0 && !UTCharacter->IsFiringDisabled())
	{
		LastWeaponPrevNextTime = GetWorld()->GetTimeSeconds();
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
				if (Weap != CurrentWeapon && Weap->CanSwitchTo())
				{
					if (Weap->FollowsInList(CurrentWeapon) == bPrev)
					{
						// remember last weapon in list as possible wraparound choice
						if (WraparoundChoice == NULL || (Weap->FollowsInList(WraparoundChoice) == bPrev))
						{
							WraparoundChoice = Weap;
						}
					}
					else if (Best == NULL || (Weap->FollowsInList(Best) == bPrev))
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

		UUTProfileSettings* ProfileSettings = GetProfileSettings();
		bool bAutoWeaponSwitch = ProfileSettings ? ProfileSettings->bAutoWeaponSwitch : true;
		if (!bAutoWeaponSwitch && CurWeapon)
		{
			// if holding default weapon in player start volumes, force weapon switch 
			AUTGameVolume* GV = UTCharacter->UTCharacterMovement ? Cast<AUTGameVolume>(UTCharacter->UTCharacterMovement->GetPhysicsVolume()) : nullptr;
			if (GV)
			{
				if (GV->bIsTeamSafeVolume || (GetWorld()->GetTimeSeconds() - LeftSpawnVolumeTime < 0.3f))
				{
					for (int32 i = 0; i < UTCharacter->DefaultCharacterInventory.Num(); i++)
					{
						if (CurWeapon->GetClass() == UTCharacter->DefaultCharacterInventory[i])
						{
							bAutoWeaponSwitch = true;
							break;
						}
					}
				}
			}
		}

		if (CurWeapon == NULL || (bAutoWeaponSwitch && GetWeaponAutoSwitchPriority(TestWeapon) > GetWeaponAutoSwitchPriority(CurWeapon)) )
		{
			if (CurWeapon && UTCharacter->IsPendingFire(CurWeapon->GetCurrentFireMode()))
			{
				UTCharacter->PendingAutoSwitchWeapon = TestWeapon;
			}
			else
			{
				UTCharacter->SwitchWeapon(TestWeapon);
			}
		}
	}
}

void AUTPlayerController::RefreshWeaponGroups()
{
/*
	if (UTCharacter == nullptr)  return;

	TArray<int32> GroupSlotIndexes;
	GroupSlotIndexes.AddZeroed(10);

	for (TInventoryIterator<AUTWeapon> It(UTCharacter); It; ++It)
	{
		AUTWeapon* Weap = *It;
		if (Weap != nullptr && GroupSlotIndexes.IsValidIndex(Weap->Group))
		{
			Weap->GroupSlot = GroupSlotIndexes[Weap->Group];
			GroupSlotIndexes[Weap->Group]++;
		}
	}
*/
}

void AUTPlayerController::SwitchWeaponGroup(int32 Group)
{
	if (UTCharacter != NULL && IsLocalPlayerController() && UTCharacter->TauntCount == 0 && !UTCharacter->IsRagdoll())
	{
		// if current weapon isn't in the specified group, pick lowest GroupSlot in that group
		// if it is, then pick next highest slot, or wrap around to lowest if no higher slot
		AUTWeapon* CurrWeapon = (UTCharacter->GetPendingWeapon() != NULL) ? UTCharacter->GetPendingWeapon() : UTCharacter->GetWeapon();
		AUTWeapon* LowestSlotWeapon = NULL;
		AUTWeapon* NextSlotWeapon = NULL;

		int32 CurrentGroup = GetWeaponGroup(CurrWeapon);
		for (TInventoryIterator<AUTWeapon> It(UTCharacter); It; ++It)
		{
			AUTWeapon* Weap = *It;
			if (Weap != UTCharacter->GetWeapon() && Weap->CanSwitchTo() && Weap != CurrWeapon)
			{
				int32 WeapGroup = GetWeaponGroup(Weap);
				if (WeapGroup == Group)
				{
					if (LowestSlotWeapon == NULL || LowestSlotWeapon->GroupSlot > Weap->GroupSlot)
					{
						LowestSlotWeapon = Weap;
					}
					if (CurrWeapon != NULL && CurrentGroup == Group && Weap->GroupSlot > CurrWeapon->GroupSlot && (NextSlotWeapon == NULL || NextSlotWeapon->GroupSlot > Weap->GroupSlot))
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
	AUTGameState* UTGameState = GetWorld()->GetGameState<AUTGameState>();
	int32 AdjustedGroup = Group -1;
	if ( UTGameState && UTPlayerState && 
		(UTPlayerState->bOutOfLives || (UTCharacter ? UTCharacter->IsDead() : (GetPawn() == NULL))) &&
		(AdjustedGroup >= 0 && UTGameState->SpawnPacks.IsValidIndex(AdjustedGroup)) )
	{
		UE_LOG(UT,Log,TEXT("ServerSetLoadoutPack %s"),*UTGameState->SpawnPacks[AdjustedGroup].PackTag.ToString());
		UTPlayerState->ServerSetLoadoutPack(UTGameState->SpawnPacks[AdjustedGroup].PackTag);
	}
	else
	{
		SwitchWeaponGroup(Group);
	}
}

void AUTPlayerController::DemoRestart()
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver && !DemoDriver->IsFastForwarding())
	{
		OnDemoSeeking();
		DemoDriver->GotoTimeInSeconds(0);
	}
}

void AUTPlayerController::DemoSeek(float DeltaSeconds)
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver && !DemoDriver->IsFastForwarding())
	{
		OnDemoSeeking();
		DemoDriver->GotoTimeInSeconds(DemoDriver->DemoCurrentTime + DeltaSeconds);
	}
}

void AUTPlayerController::DemoGoTo(float Seconds)
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver && !DemoDriver->IsFastForwarding())
	{
		OnDemoSeeking();
		DemoDriver->GotoTimeInSeconds(Seconds);
	}
}

void AUTPlayerController::DemoGoToLive()
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver && !DemoDriver->IsFastForwarding())
	{
		OnDemoSeeking();
		DemoDriver->JumpToEndOfLiveReplay();
	}
}

void AUTPlayerController::OnDemoSeeking()
{
	if (MyUTHUD != NULL)
	{
		MyUTHUD->ToggleScoreboard(false);
	}
}

void AUTPlayerController::UTDemoPause()
{
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver)
	{
		AWorldSettings* const WorldSettings = GetWorldSettings();

		if (WorldSettings->Pauser == nullptr)
		{
			WorldSettings->Pauser = PlayerState;
		}
		else
		{
			WorldSettings->Pauser = nullptr;
		}
	}
}

void AUTPlayerController::DemoTimeDilation(float DeltaAmount)
{
	static float DilationLUT[5] = { 0.1f, 0.5f, 1.0f, 2.0f, 4.0f };

	if ( DeltaAmount > 0 )
	{
		DilationIndex = FMath::Clamp( DilationIndex + 1, 0, 4 );
	}
	else if ( DeltaAmount < 0 )
	{
		DilationIndex = FMath::Clamp( DilationIndex - 1, 0, 4 );
	}

	GetWorldSettings()->DemoPlayTimeDilation = DilationLUT[DilationIndex];
}

void AUTPlayerController::DemoSetTimeDilation(float Amount)
{
	Amount = FMath::Clamp(Amount, 0.1f, 5.0f);
	GetWorldSettings()->DemoPlayTimeDilation = Amount;
}

void AUTPlayerController::ViewPlayerNum(int32 Index, uint8 TeamNum)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL)
	{
		APlayerState** PlayerToView = NULL;
		if ((TeamNum == 255) || !GS->Teams.IsValidIndex(TeamNum))
		{
			if (TeamNum == 1)
			{
				Index += 5;
			}
			int32 MaxSpectatingId = GS->GetMaxSpectatingId();
			while ((Index <= MaxSpectatingId) && (PlayerToView == NULL))
			{
				PlayerToView = GS->PlayerArray.FindByPredicate([=](const APlayerState* TestItem) -> bool
				{
					const AUTPlayerState* PS = Cast<AUTPlayerState>(TestItem);
					return (PS != NULL && PS->SpectatingID == Index);
				});
				Index += 10;
			}
		}
		else
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
			if (PlayerState && !PlayerState->bOnlySpectator && !GS->OnSameTeam(this, *PlayerToView))
			{
				// can't view opposing players if not spectator
				return;
			}
			if (Cast<AUTPlayerState>(*PlayerToView) && Cast<AUTPlayerState>(*PlayerToView)->bOutOfLives)
			{
				return;
			}
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

void AUTPlayerController::ToggleMinimap()
{
	if (MyUTHUD)
	{
		MyUTHUD->bDrawMinimap = !MyUTHUD->bDrawMinimap;
	}
}

void AUTPlayerController::ToggleShowBinds()
{
	bShowCameraBinds = !bShowCameraBinds;
}

void AUTPlayerController::ToggleShowTimers()
{
	bShowPowerupTimers = !bShowPowerupTimers;
}

void AUTPlayerController::ViewNextPlayer()
{
	bAutoCam = false;
	BehindView(bSpectateBehindView);
	ServerViewNextPlayer();
}

void AUTPlayerController::ViewPowerup(FString PowerupName)
{
	for (FActorIterator It(GetWorld()); It; ++It)
	{
		AUTPickup* Pickup = Cast<AUTPickup>(*It);
		if (Pickup && (Pickup->GetName() == PowerupName))
		{
			bAutoCam = false;
			if (Pickup->Camera)
			{
				AActor* NewViewTarget = (GetSpectatorPawn() != NULL) ? GetSpectatorPawn() : SpawnSpectatorPawn();
				NewViewTarget->SetActorLocationAndRotation(Pickup->Camera->GetActorLocation(), Pickup->Camera->GetActorRotation());
				ResetCameraMode();
				SetViewTarget(NewViewTarget);
				SetControlRotation(Pickup->Camera->GetActorRotation());
				ServerViewSelf();
			}
			else
			{
				SetViewTarget(Pickup);
			}
			break;
		}
	}
}

bool AUTPlayerController::ServerViewFlagHolder_Validate(uint8 TeamIndex)
{
	return true;
}

void AUTPlayerController::ServerViewFlagHolder_Implementation(uint8 TeamIndex)
{
	if (IsInState(NAME_Spectating) && (PlayerState == NULL || PlayerState->bOnlySpectator || GetTeamNum() == TeamIndex))
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
	if (IsInState(NAME_Spectating) && PS != NULL && GetWorld()->GetAuthGameMode() != NULL && GetWorld()->GetAuthGameMode()->CanSpectate(this, PS))
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
	if (IsInState(NAME_Spectating))
	{
		AUTCTFGameState* CTFGameState = GetWorld()->GetGameState<AUTCTFGameState>();
		if (CTFGameState && (CTFGameState->FlagBases.Num() > Index) && CTFGameState->FlagBases[Index] && CTFGameState->FlagBases[Index]->MyFlag && UTPlayerState && (UTPlayerState->bOnlySpectator || CTFGameState->OnSameTeam(this, CTFGameState->FlagBases[Index]->MyFlag)))
		{
			SetViewTarget(CTFGameState->FlagBases[Index]->MyFlag);
		}
	}
}

void AUTPlayerController::ViewCamera(int32 Index)
{
	if (IsInState(NAME_Spectating) && PlayerState->bOnlySpectator)
	{
		int32 CamCount = 0;
		for (FActorIterator It(GetWorld()); It; ++It)
		{
			AUTSpectatorCamera* Cam = Cast<AUTSpectatorCamera>(*It);
			if (Cam)
			{
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
				CamCount++;
			}
		}
	}
}

void AUTPlayerController::ViewProjectile()
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		if (Cast<AUTProjectile>(GetViewTarget()) && LastSpectatedPlayerId >= 0)
		{
			// toggle away from projectile cam
			for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
			{
				APawn* PawnIter = *Iterator;
				if (PawnIter != nullptr)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(PawnIter->PlayerState);
					if (PS && PS->SpectatingID == LastSpectatedPlayerId)
					{
						bAutoCam = false;
						ViewPawn(*Iterator);
						break;
					}
				}
			}
		}
		else
		{
			if (LastSpectatedPlayerId < 0)
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
					AUTPlayerState* PS = Cast<AUTPlayerState>(ViewedCharacter->PlayerState);
					if (PS)
					{
						LastSpectatedPlayerId = PS->SpectatingID;
					}
				}
			}
			bAutoCam = false;
			ServerViewProjectileShim();
		}
	}
}

void AUTPlayerController::ServerViewProjectileShim()
{
	ServerViewProjectile();
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
		if (!ViewedCharacter)
		{
			ViewProjectileTime = 0.f;
		}
		else if (ViewedCharacter->LastFiredProjectile && !ViewedCharacter->LastFiredProjectile->IsPendingKillPending())
		{
			SetViewTarget(ViewedCharacter->LastFiredProjectile);
			ViewProjectileTime = 0.f;
		}
		else if (ViewProjectileTime == 0.f)
		{
			ViewProjectileTime = GetWorld()->GetTimeSeconds() + 8.f;
		}
	}
}

void AUTPlayerController::TogglePlayerInfo()
{
	if (PlayerState && PlayerState->bOnlySpectator)
	{
		//Find the playerstate of the player we are currently spectating
		AUTPlayerState* PS = Cast<AUTPlayerState>(GetViewTarget());

		if (PS == nullptr)
		{
			APawn* PawnViewTarget = Cast<APawn>(GetViewTarget());
			if (PawnViewTarget)
			{
				PS = Cast<AUTPlayerState>(PawnViewTarget->PlayerState);
			}
		}

		UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
		if (PS != nullptr && LP != nullptr && PS != PlayerState)
		{
			LP->ShowPlayerInfo(PS);
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
		new(DeferredFireInputs)FDeferredFireInput(0, true);
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
		new(DeferredFireInputs)FDeferredFireInput(0, false);
	}
}

void AUTPlayerController::OnAltFire()
{
	if (GetPawn() != NULL)
	{
		new(DeferredFireInputs)FDeferredFireInput(1, true);
	}
	else if (GetWorld()->GetGameState() != NULL && GetWorld()->GetGameState()->HasMatchStarted() && IsInState(NAME_Spectating) )
	{
		PlayMenuSelectSound();
		if (PlayerState && PlayerState->bOnlySpectator)
		{
			bAutoCam = false;
			ViewSelf();
		}
		else if (bPlayerIsWaiting)
		{
			ServerRestartPlayer();
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
		new(DeferredFireInputs)FDeferredFireInput(1, false);
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
	if (UTCharacter != NULL && !IsMoveInputIgnored())
	{
		UTCharacter->bPressedJump = true;
		UTCharacter->UTCharacterMovement->UpdateWallSlide(true);
	}
}

void AUTPlayerController::JumpRelease()
{
	if (UTCharacter)
	{
		UTCharacter->UTCharacterMovement->UpdateWallSlide(false);
	}
}

void AUTPlayerController::Crouch()
{
	if (!IsMoveInputIgnored())
	{
		bIsHoldingFloorSlide = bCrouchTriggersSlide;
		if (UTCharacter)
		{
			UTCharacter->Crouch(false);
		}
	}
}

void AUTPlayerController::UnCrouch()
{
	if (bCrouchTriggersSlide)
	{
		bIsHoldingFloorSlide = false;
	}
	if (UTCharacter)
	{
		UTCharacter->UnCrouch(false);
	}
}

void AUTPlayerController::Slide()
{
	if (!IsMoveInputIgnored())
	{
		bIsHoldingFloorSlide = true;
		UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
		if (MyCharMovement)
		{
			MyCharMovement->HandleSlideRequest();
		}
	}
}

void AUTPlayerController::StopSlide()
{
	bIsHoldingFloorSlide = false;
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement)
	{
		MyCharMovement->UpdateFloorSlide(false);
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

void AUTPlayerController::HearSound(USoundBase* InSoundCue, AActor* SoundPlayer, const FVector& SoundLocation, bool bStopWhenOwnerDestroyed, bool bAmplifyVolume, ESoundAmplificationType AmpType)
{
	bool bIsOccluded = false; 
	float MaxAudibleDistance = InSoundCue->GetAttenuationSettingsToApply() ? InSoundCue->GetAttenuationSettingsToApply()->GetMaxDimension() : 15000.f;
/*	if (true)//(GetNetMode() == NM_DedicatedServer)
	{
		if (!InSoundCue->GetAttenuationSettingsToApply())
		{
			UE_LOG(UT, Warning, TEXT("NO ATTENUATION SETTINGS FOR %s"), *InSoundCue->GetName());
		}
		else
		{
			if (!InSoundCue->AttenuationSettings)
			{
			UE_LOG(UT, Warning, TEXT("NO ATTENUATION SETTINGS OBJECT FOR %s"), *InSoundCue->GetName());
			}
			
			if (InSoundCue->GetAttenuationSettingsToApply()->bAttenuateWithLPF)
			{
				UE_LOG(UT, Warning, TEXT("AttenuateWithLPF FOR %s"), *InSoundCue->GetName());
			}
			if (InSoundCue->GetAttenuationSettingsToApply()->bEnableListenerFocus)
			{
				UE_LOG(UT, Warning, TEXT("bEnableListenerFocus FOR %s"), *InSoundCue->GetName());
			}
			if (InSoundCue->GetAttenuationSettingsToApply()->bEnableOcclusion)
			{
				UE_LOG(UT, Warning, TEXT("bEnableOcclusion FOR %s"), *InSoundCue->GetName());
			}
			if (InSoundCue->GetAttenuationSettingsToApply()->bUseComplexCollisionForOcclusion)
			{
				UE_LOG(UT, Warning, TEXT("bUseComplexCollisionForOcclusion FOR %s"), *InSoundCue->GetName());
			}
		}
	}*/
	if (SoundPlayer == this || (GetViewTarget() != NULL && (bAmplifyVolume || (SoundPlayer == this) || (SoundPlayer == GetViewTarget()) || MaxAudibleDistance >= (SoundLocation - GetViewTarget()->GetActorLocation()).Size())))
	{
		// we don't want to replicate the location if it's the same as Actor location (so the sound gets played attached to the Actor), but we must if the source Actor isn't relevant
		UNetConnection* Conn = Cast<UNetConnection>(Player);
		FVector RepLoc = (SoundPlayer != NULL && SoundPlayer->GetActorLocation() == SoundLocation && (Conn == NULL || Conn->ActorChannels.Contains(SoundPlayer))) ? FVector::ZeroVector : SoundLocation;
		ClientHearSound(InSoundCue, SoundPlayer, RepLoc, bStopWhenOwnerDestroyed, bAmplifyVolume, AmpType);
	}
}

void AUTPlayerController::ClientHearSound_Implementation(USoundBase* TheSound, AActor* SoundPlayer, FVector_NetQuantize SoundLocation, bool bStopWhenOwnerDestroyed, bool bAmplifyVolume, ESoundAmplificationType AmpType)
{
	// Don't play RPC sounds during fast forward
	UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
	if (DemoDriver && DemoDriver->IsFastForwarding())
	{
		return;
	}

	if (TheSound != NULL && (SoundPlayer != NULL || !SoundLocation.IsZero()))
	{
		FAudioDevice* AudioDevice = GetWorld()->GetAudioDevice();
		bool bHRTFEnabled = (AudioDevice != nullptr && AudioDevice->IsHRTFEnabledForAll());
		FCustomSoundAmplification CustomAmp = FCustomSoundAmplification();
		switch (AmpType)
		{
			case SAT_Footstep: CustomAmp = FootStepAmp; break;
			case SAT_WeaponFire: CustomAmp = WeaponFireAmp; break;
			case SAT_PainSound: CustomAmp = PainSoundAmp; break;
			case SAT_WeaponFoley: CustomAmp = WeaponFoleyAmp; break;
		}
/*
		FString AmpTypName = "NO AMP";
		switch (AmpType)
		{
		case SAT_Footstep: AmpTypName = "FootStepAmp"; break;
		case SAT_WeaponFire: AmpTypName = "WeaponFireAmp"; break;
		case SAT_PainSound: AmpTypName = "PainSoundAmp"; break;
		case SAT_WeaponFoley: AmpTypName = "WeaponFoleyAmp"; break;
		}
		if (AmpType != SAT_Footstep)
		{
			UE_LOG(UT, Warning, TEXT("%s  attenuation %s amp type %s"), *TheSound->GetName(), TheSound->AttenuationSettings ? *TheSound->AttenuationSettings->GetName() : TEXT("NONE"), *AmpTypName);
		}
*/
		if (!bHRTFEnabled && (SoundPlayer == this || SoundPlayer == GetViewTarget()))
		{
			// no attenuation/spatialization, full volume
			FActiveSound NewActiveSound;
			NewActiveSound.SetWorld(GetWorld());
			NewActiveSound.SetSound(TheSound);

			NewActiveSound.VolumeMultiplier = CustomAmp.OwnVolumeMultiplier;
			NewActiveSound.PitchMultiplier = CustomAmp.OwnPitchMultiplier;

			NewActiveSound.RequestedStartTime = 0.0f;

			NewActiveSound.bLocationDefined = false;
			NewActiveSound.bIsUISound = false;
			NewActiveSound.bHasAttenuationSettings = false;
			NewActiveSound.bAllowSpatialization = false;
			
			if (AudioDevice)
			{
				AudioDevice->AddNewActiveSound(NewActiveSound);
			}
		}
		else
		{
			bool bSkipIfOccluded = (AmpType == SAT_WeaponFoley) || (AmpType == SAT_Footstep);
			bool bSkipIfTeammateOccluded = (AmpType == SAT_Footstep);
			bool bSameTeam = false;
			bool bInstigatedSound = false;
			USoundAttenuation* AttenuationOverride = NULL;
			float VolumeMultiplier = 1.f;
			float PitchMultiplier = 1.f;
			bool bOverrideSoundClass = false;
			if (bAmplifyVolume)
			{
				// target
				AttenuationOverride = CustomAmp.TargetAttenuation;
				VolumeMultiplier = CustomAmp.TargetVolumeMultiplier;
				PitchMultiplier = CustomAmp.TargetPitchMultiplier;
				bOverrideSoundClass = (AmpType == SAT_PainSound);
			}
			else if (SoundPlayer && (SoundPlayer->GetInstigator() == GetViewTarget()))
			{
				// instigator
				AttenuationOverride = CustomAmp.InstigatorAttenuation;
				VolumeMultiplier = CustomAmp.InstigatorVolumeMultiplier;
				PitchMultiplier = CustomAmp.InstigatorPitchMultiplier;
				bInstigatedSound = true;
			}
			else
			{
				// check if same team
				AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GameState);
				bSameTeam = (GS && GS->OnSameTeam(this, SoundPlayer));
				if (bSameTeam)
				{
					AttenuationOverride = CustomAmp.TeammateAttenuation;
					VolumeMultiplier = CustomAmp.TeammateVolumeMultiplier;
					PitchMultiplier = CustomAmp.TeammatePitchMultiplier;
				}
				else if (AmpType == SAT_Footstep)
				{
					FVector ViewPoint;
					FRotator ViewRotation;
					GetActorEyesViewPoint(ViewPoint, ViewRotation);
					if (SoundPlayer && ((ViewRotation.Vector() | (SoundPlayer->GetActorLocation() - ViewPoint).GetSafeNormal()) < 0.7f))
					{
						VolumeMultiplier = 2.f;
					}
				}
			}
			if (!bAmplifyVolume && !bInstigatedSound && (bSkipIfOccluded || (bSkipIfTeammateOccluded && bSameTeam) || (CustomAmp.OccludedAttenuation != nullptr)) && (!SoundLocation.IsZero() || SoundPlayer))
			{
				// if further than 0.65f audible radius, skip if occluded
				float MaxAudibleDistance = TheSound->GetAttenuationSettingsToApply() ? TheSound->GetAttenuationSettingsToApply()->GetMaxDimension() : 4000.f;
				if (bSameTeam)
				{
					MaxAudibleDistance *= CustomAmp.OccludedAttenuation ? 1.f : 0.5f;
				}
				FVector ViewPoint;
				FRotator ViewRotation;
				GetActorEyesViewPoint(ViewPoint, ViewRotation);
				static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
				FCollisionQueryParams CollisionParms(NAME_LineOfSight, true, SoundPlayer);
				CollisionParms.AddIgnoredActor(GetViewTarget());
				if (SoundPlayer)
				{
					CollisionParms.AddIgnoredActor(SoundPlayer);
				}

				FVector PlaySoundLocation = SoundLocation.IsZero() ? SoundPlayer->GetActorLocation() : SoundLocation;
				bool bHit = GetWorld()->LineTraceTestByChannel(ViewPoint, PlaySoundLocation, ECC_Visibility, CollisionParms);
				if (bHit)
				{
					if (CustomAmp.OccludedAttenuation != nullptr)
					{
						AttenuationOverride = CustomAmp.OccludedAttenuation;
					}
					else if (0.65f * MaxAudibleDistance < (PlaySoundLocation - ViewPoint).Size())
					{
						return;
					}
				}
			}

			UAudioComponent* AC = nullptr;
			if (!SoundLocation.IsZero() && (SoundPlayer == NULL || SoundLocation != SoundPlayer->GetActorLocation()))
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), TheSound, SoundLocation, VolumeMultiplier, PitchMultiplier, 0.0f, AttenuationOverride);
			}
			else if (SoundPlayer != NULL)
			{
				AC = UGameplayStatics::SpawnSoundAttached(TheSound, SoundPlayer->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, bStopWhenOwnerDestroyed, VolumeMultiplier, PitchMultiplier, 0.0f, AttenuationOverride);
			}
			if (bOverrideSoundClass && AC)
			{
				AC->SoundClassOverride = PriorityFXSoundClass;
			}
		}
	}
}

void AUTPlayerController::ClientWarnEnemyBehind_Implementation(AUTPlayerState* TeamPS, AUTCharacter* Targeter)
{
	if (Targeter && (GetWorld()->GetTimeSeconds() - Targeter->GetLastRenderTime() > 5.f) && TeamPS && TeamPS->CharacterVoice)
	{
		int32 Switch = TeamPS->CharacterVoice.GetDefaultObject()->GetStatusIndex(StatusMessage::BehindYou);
		if (Switch < 0)
		{
			UE_LOG(UT, Warning, TEXT("No valid index found for BEHIND YOU"));
			// no valid index found (NewStatus was not a valid selection)
			return;
		}

		ClientReceiveLocalizedMessage(TeamPS->CharacterVoice, Switch, TeamPS, PlayerState, NULL);
	}
}

void AUTPlayerController::CheckDodge(float LastTapTime, float MaxClickTime, bool bForward, bool bBack, bool bLeft, bool bRight)
{
	UUTCharacterMovement* MyCharMovement = UTCharacter ? UTCharacter->UTCharacterMovement : NULL;
	if (MyCharMovement != NULL && !IsMoveInputIgnored() && (bIsHoldingDodge || (GetWorld()->GetRealTimeSeconds() - LastTapTime < MaxClickTime)))
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
	if (MyCharMovement != NULL && !IsMoveInputIgnored())
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
		if (!MyCharMovement->IsMovingOnGround() && MyCharMovement->bPressedDodgeForward)
		{
			float PawnCapsuleRadius, PawnCapsuleHalfHeight;
			UTCharacter->GetCapsuleComponent()->GetScaledCapsuleSize(PawnCapsuleRadius, PawnCapsuleHalfHeight);
			float TraceBoxSize = FMath::Min(0.25f*PawnCapsuleHalfHeight, 0.7f*PawnCapsuleRadius);
			FVector TraceStart = UTCharacter->GetActorLocation();
			TraceStart.Z -= 0.5f*TraceBoxSize;
			static const FName DodgeTag = FName(TEXT("Dodge"));
			FCollisionQueryParams QueryParams(DodgeTag, false, UTCharacter);
			FHitResult Result;
			float DodgeTraceDist = MyCharMovement->WallDodgeTraceDist + PawnCapsuleRadius - 0.5f*TraceBoxSize;

			FVector DodgeDir, DodgeCross;
			MyCharMovement->GetDodgeDirection(DodgeDir, DodgeCross);

			// if chosen direction is not valid wall dodge, look for alternate
			FVector TraceEnd = TraceStart - DodgeTraceDist*DodgeDir;
			bool bBlockingHit = GetWorld()->SweepSingleByChannel(Result, TraceStart, TraceEnd, FQuat::Identity, MyCharMovement->UpdatedComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(TraceBoxSize), QueryParams);
			if (!bBlockingHit)
			{
				// try sides
				MyCharMovement->bPressedDodgeForward = false;
				MyCharMovement->bPressedDodgeRight = true;
				MyCharMovement->GetDodgeDirection(DodgeDir, DodgeCross);
				TraceEnd = TraceStart - DodgeTraceDist*DodgeDir;
				bBlockingHit = GetWorld()->SweepSingleByChannel(Result, TraceStart, TraceEnd, FQuat::Identity, MyCharMovement->UpdatedComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(TraceBoxSize), QueryParams);
				if (!bBlockingHit)
				{
					MyCharMovement->bPressedDodgeRight = false;
					MyCharMovement->bPressedDodgeLeft = true;
					MyCharMovement->GetDodgeDirection(DodgeDir, DodgeCross);
					TraceEnd = TraceStart - DodgeTraceDist*DodgeDir;
					bBlockingHit = GetWorld()->SweepSingleByChannel(Result, TraceStart, TraceEnd, FQuat::Identity, MyCharMovement->UpdatedComponent->GetCollisionObjectType(), FCollisionShape::MakeSphere(TraceBoxSize), QueryParams);
					if (!bBlockingHit)
					{
						MyCharMovement->bPressedDodgeLeft = false;
						MyCharMovement->bPressedDodgeBack = true;
					}
				}
			}
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
	LastTapRightTime = -10.f;
	LastTapLeftTime = -10.f;
	CheckDodge(LastTapForwardTime, MaxDodgeClickTime, true, false, false, false);
	LastTapForwardTime = GetWorld()->GetRealTimeSeconds();
}

void AUTPlayerController::OnTapBack()
{
	LastTapForwardTime = -10.f;
	LastTapRightTime = -10.f;
	LastTapLeftTime = -10.f;
	CheckDodge(LastTapBackTime, MaxDodgeClickTime, false, true, false, false);
	LastTapBackTime = GetWorld()->GetRealTimeSeconds();
}

void AUTPlayerController::OnTapLeft()
{
	LastTapRightTime = -10.f;
	LastTapBackTime = -10.f;
	LastTapForwardTime = -10.f;
	CheckDodge(LastTapLeftTime, MaxDodgeClickTime, false, false, true, false);
	LastTapLeftTime = GetWorld()->GetRealTimeSeconds();
}

void AUTPlayerController::OnTapRight()
{
	LastTapLeftTime = -10.f;
	LastTapBackTime = -10.f;
	LastTapForwardTime = -10.f;
	CheckDodge(LastTapRightTime, MaxDodgeClickTime, false, false, false, true);
	LastTapRightTime = GetWorld()->GetRealTimeSeconds();
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
		for (AUTPickup* Pickup : WS->PerPlayerPickups)
		{
			bool bTaken = !Pickup->State.bActive;

			if (RecentPerPlayerPickups.Contains(Pickup))
			{
				if (Pickup->IsTaken(GetPawn()))
				{
					bTaken = true;
					Pickup->TimerEffect->SetFloatParameter(NAME_Progress, 1.0f - Pickup->GetRespawnTimeOffset(GetPawn()) / Pickup->RespawnTime);
				}
				else
				{
					Pickup->PlayRespawnEffects();
					RecentPerPlayerPickups.Remove(Pickup);
				}
			}
			Pickup->AddHiddenComponents(bTaken, HiddenComponents);
		}
	}

	// hide all components that shouldn't be shown in the current 1P/3P state
	// with bOwnerNoSee/bOnlyOwnerSee not being propagated to children this method is much easier to maintain
	// although slightly less efficient
	AUTCharacter* P = Cast<AUTCharacter>(GetViewTarget());
	if (IsBehindView())
	{
		// hide first person weapon
		if (P != NULL)
		{
			HiddenComponents.Add(P->FirstPersonMesh->ComponentId);
			if (P->GetWeapon() != NULL)
			{
				TArray<UMeshComponent*> Meshes = P->GetWeapon()->Get1PMeshes();
				for (UMeshComponent* WeapMesh : Meshes)
				{
					if (WeapMesh != NULL)
					{
						HideComponentTree(WeapMesh, HiddenComponents);
					}
				}
			}
			if (!P->GetMesh()->bCastCapsuleIndirectShadow)
			{
				P->GetMesh()->bCastCapsuleIndirectShadow = true;
				P->GetMesh()->MarkRenderStateDirty();
			}
		}
	}
	else if (P != NULL)
	{
		// hide first person mesh (but not attachments) if hidden weapons
		if (GetWeaponHand() == EWeaponHand::HAND_Hidden || (P->GetWeapon() != NULL && P->GetWeapon()->ZoomState != EZoomState::EZS_NotZoomed))
		{
			HiddenComponents.Add(P->FirstPersonMesh->ComponentId);
			if (P->GetWeapon() != NULL)
			{
				TArray<UMeshComponent*> Meshes = P->GetWeapon()->Get1PMeshes();
				for (UMeshComponent* WeapMesh : Meshes)
				{
					if (WeapMesh != NULL)
					{
						HiddenComponents.Add(WeapMesh->ComponentId);
					}
				}
			}
		}
		else if (P->GetWeapon() == NULL || P->GetWeapon()->HandsAttachSocket == NAME_None)
		{
			// weapon doesn't use hands
			HiddenComponents.Add(P->FirstPersonMesh->ComponentId);
		}
		// hide third person character model
		HideComponentTree(P->GetMesh(), HiddenComponents);
		if (P->GetMesh()->bCastCapsuleIndirectShadow)
		{
			P->GetMesh()->bCastCapsuleIndirectShadow = false;
			P->GetMesh()->MarkRenderStateDirty();
		}

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

	int32 MyVisibilityMask = 0;
	if (GetUTCharacter())
	{
		MyVisibilityMask = GetUTCharacter()->VisibilityMask;	
	}

	// hide other pawns' first person hands/weapons
	// hide outline if visible but not to my team
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (It->IsValid() && It->Get() != GetViewTarget() && It->Get() != GetPawn())
		{
			AUTCharacter* OtherP = Cast<AUTCharacter>(It->Get());
			if (OtherP != NULL)
			{
				HideComponentTree(OtherP->FirstPersonMesh, HiddenComponents);
				if (PlayerState != NULL && !PlayerState->bOnlySpectator)
				{
					if (OtherP->VisibilityMask > 0 && (OtherP->VisibilityMask & MyVisibilityMask) == 0)
					{
						HideComponentTree(OtherP->GetMesh(), HiddenComponents);
					}
				}
				if (OtherP->GetCustomDepthMesh() != NULL && !OtherP->IsOutlined(GetTeamNum()))
				{
					HideComponentTree(OtherP->GetCustomDepthMesh(), HiddenComponents);
					if (OtherP->GetWeaponAttachment() != NULL)
					{
						HideComponentTree(OtherP->GetWeaponAttachment()->GetCustomDepthMesh(), HiddenComponents);
					}
				}
			}
		}
	}

	// hide aura for enemy flag
	AUTCTFGameState* CTFGS = GetWorld()->GetGameState<AUTCTFGameState>();
	if (CTFGS != NULL)
	{
		for (AUTCTFFlagBase* Base : CTFGS->FlagBases)
		{
			if (Base != NULL)
			{
				AUTCTFFlag* Flag = Cast<AUTCTFFlag>(Base->GetCarriedObject());
				if (Flag != NULL && (Flag->GetTeamNum() != GetTeamNum() || Flag->ObjectState == CarriedObjectState::Home) && Flag->AuraSphere != NULL)
				{
					HiddenComponents.Add(Flag->AuraSphere->ComponentId);
				}
			}
		}
	}
}

void AUTPlayerController::ToggleScoreboard(bool bShow)
{
	if (MyUTHUD)
	{
		MyUTHUD->ToggleScoreboard(bShow);
	}
}

void AUTPlayerController::ClientToggleScoreboard_Implementation(bool bShow)
{
	ToggleScoreboard(bShow);
}

void AUTPlayerController::OnRep_HUDClass()
{
	// First, create the HUD
	ClientSetHUD_Implementation(HUDClass);
	MyUTHUD = Cast<AUTHUD>(MyHUD);
}

void AUTPlayerController::OnShowScores()
{
	AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GameState);
	if (!GS || (GS->IsMatchInProgress() && !GS->IsMatchIntermission()))
	{
		ToggleScoreboard(true);
	}
	else
	{
		// toggles on and off during intermissions
		if (MyUTHUD)
		{
			ToggleScoreboard(!MyUTHUD->bShowScores);
		}
	}
}

void AUTPlayerController::OnHideScores()
{
	AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GameState);
	if (!GS || (GS->IsMatchInProgress() && !GS->IsMatchIntermission()))
	{
		ToggleScoreboard(false);
	}
}

AUTCharacter* AUTPlayerController::GetUTCharacter()
{
	return UTCharacter;
}


bool AUTPlayerController::ServerToggleWarmup_Validate()
{
	return true;
}

void AUTPlayerController::ServerToggleWarmup_Implementation()
{
	AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GameState);
	if (!GS || (GS->GetMatchState() != MatchState::WaitingToStart) || !UTPlayerState || !UTPlayerState->bReadyToPlay)
	{
		return;
	}
	UTPlayerState->bIsWarmingUp = !UTPlayerState->bIsWarmingUp;
	UTPlayerState->ForceNetUpdate();
	if (UTPlayerState->bIsWarmingUp)
	{
		if (!IsFrozen())
		{
			Super::ServerRestartPlayer_Implementation();
		}
	}
	else
	{
		AUTCharacter* Char = Cast<AUTCharacter>(GetPawn());
		if (GetPawn() && (Char == nullptr))
		{
			GetPawn()->Destroy();
			Char = Cast<AUTCharacter>(GetPawn());
		}
		if (Char != NULL)
		{
			Char->PlayerSuicide();
		}
		bPlayerIsWaiting = true;
		ViewStartSpot();
	}
}

void AUTPlayerController::ServerRestartPlayer_Implementation()
{
	bUseAltSpawnPoint = false;

	if (UTPlayerState != nullptr)
	{
		UTPlayerState->bChosePrimaryRespawnChoice = true;
		UTPlayerState->ForceNetUpdate();
	}
	AUTGameMode* UTGM = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (!UTGM)
	{
		// this is a newly disconnected client
		return;
	}
	// Ready up if match hasn't started and not a ranked match
	if (!UTGM->HasMatchStarted() && !UTGM->bRankedSession && UTPlayerState && !UTPlayerState->bIsWarmingUp)
	{
		if (UTPlayerState->bCaster)
		{
			//For casters, all players need to be ready before the caster can be ready. This avoids the game starting if the caster has been mashing buttons while players are getting ready
			AUTGameState* GS = Cast<AUTGameState>(GetWorld()->GameState);
			if (UTPlayerState->bCaster && GS != nullptr && GS->AreAllPlayersReady())
			{
				UTPlayerState->SetReadyToPlay(true);
				UTPlayerState->ForceNetUpdate();
			}
		}
		else if ((UTGM->GetMatchState() != MatchState::CountdownToBegin) && (UTGM->GetMatchState() != MatchState::PlayerIntro))
		{
			UTPlayerState->SetReadyToPlay(!UTPlayerState->bReadyToPlay);
			UTPlayerState->bPendingTeamSwitch = false;
			UTPlayerState->ForceNetUpdate();
		}
	}
	//Half-time ready up for caster control
	else if (UTGM->bCasterControl && UTGM->GetMatchState() == MatchState::MatchIntermission && UTPlayerState != nullptr && UTPlayerState->bCaster)
	{
		UTPlayerState->SetReadyToPlay(true);
		UTPlayerState->ForceNetUpdate();
	}
	else if (IsFrozen())
	{
		return;
	}
	else if (!UTGM->PlayerCanRestart(this))
	{
		return;
	}

	Super::ServerRestartPlayer_Implementation();
}

bool AUTPlayerController::ServerRestartPlayerAltFire_Validate()
{
	return true;
}

void AUTPlayerController::ServerSwitchTeam_Implementation()
{
	// Ranked sessions don't allow team changes
	AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (GameMode && GameMode->bRankedSession)
	{
		return;
	}

	if (UTPlayerState != NULL && UTPlayerState->Team != NULL)
	{
		uint8 NewTeam = (UTPlayerState->Team->TeamIndex + 1) % GetWorld()->GetGameState<AUTGameState>()->Teams.Num();
		if (UTPlayerState->bPendingTeamSwitch)
		{
			UTPlayerState->bPendingTeamSwitch = false;
		}
		else if (!GetWorld()->GetAuthGameMode()->HasMatchStarted())
		{
			if (UTPlayerState->bIsWarmingUp)
			{
				// no team swaps while warming up
				return;
			}
			ChangeTeam(NewTeam);
			if (UTPlayerState->bPendingTeamSwitch)
			{
				UTPlayerState->SetReadyToPlay(false);
			}
		}
		else
		{
			ChangeTeam(NewTeam);
		}
		UTPlayerState->ForceNetUpdate();
	}
}

bool AUTPlayerController::ServerSwitchTeam_Validate()
{
	return true;
}

void AUTPlayerController::ServerRestartPlayerAltFire_Implementation()
{
	bUseAltSpawnPoint = true;

	if (UTPlayerState != nullptr)
	{
		UTPlayerState->bChosePrimaryRespawnChoice = false;
		UTPlayerState->ForceNetUpdate();
	}

	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && !GS->HasMatchStarted() && UTPlayerState && !UTPlayerState->bIsWarmingUp)
	{
		if (GS->GetMatchState() != MatchState::CountdownToBegin && GS->GetMatchState() != MatchState::PlayerIntro)
		{
			ServerSwitchTeam();
		}
	}
	else 
	{
		AUTGameMode* GameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (GameMode && !GameMode->PlayerCanAltRestart(this))
		{
			return;
		}
	}

	Super::ServerRestartPlayer_Implementation();
}

bool AUTPlayerController::ServerSelectSpawnPoint_Validate(APlayerStart* DesiredStart)
{
	return true;
}
void AUTPlayerController::ServerSelectSpawnPoint_Implementation(APlayerStart* DesiredStart)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS != NULL && UTPlayerState != NULL)
	{
		if (GS->IsAllowedSpawnPoint(UTPlayerState, DesiredStart))
		{
			UTPlayerState->RespawnChoiceA = DesiredStart;
			UTPlayerState->ForceNetUpdate();
		}
		else
		{
			ClientPlayBadSelectionSound();
		}
	}
}

void AUTPlayerController::ClientPlayBadSelectionSound_Implementation()
{
	if (GetViewTarget())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), BadSelectSound, GetViewTarget()->GetActorLocation(), 1.f, 1.0f, 0.0f);
	}
}

bool AUTPlayerController::CanRestartPlayer()
{
	return Super::CanRestartPlayer() && UTPlayerState->RespawnTime <= 0.0f && (bShortConnectTimeOut || GetWorld()->TimeSeconds - CreationTime > 15.0f ||(GetNetMode() == NM_Standalone));
}

void AUTPlayerController::ResetCameraMode()
{
	bool bBehindView;
	if (IsInState(NAME_Spectating))
	{
		bBehindView = bSpectateBehindView;
	}
	else if (!bAllowPlayingBehindView && GetNetMode() != NM_Standalone && GetWorld()->WorldType != EWorldType::PIE)
	{
		bBehindView = false;
	}
	else
	{
		bBehindView = bPlayBehindView;
	}
	if (bBehindView)
	{
		SetCameraMode(FName(TEXT("FreeCam")));
	}
	else
	{
		Super::ResetCameraMode();
	}
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
		SaveConfig();
	}
	else
	{
		bPlayBehindView = bWantBehindView;
	}
	SetCameraMode(bWantBehindView ? FName(TEXT("FreeCam")) : FName(TEXT("Default")));
	if (Cast<AUTCharacter>(GetViewTarget()) != NULL)
	{
		((AUTCharacter*)GetViewTarget())->BehindViewChange(this, bWantBehindView);
	}

	// make sure we don't have leftover zoom
	if (bWantBehindView && PlayerCameraManager != NULL)
	{
		PlayerCameraManager->UnlockFOV();
		PlayerCameraManager->DefaultFOV = ConfigDefaultFOV;
	}
}

bool AUTPlayerController::IsBehindView() const
{
	if (PlayerCameraManager != NULL)
	{
		static FName NAME_FreeCam(TEXT("FreeCam"));
		static FName NAME_RallyCam(TEXT("RallyCam"));

		AUTPlayerCameraManager* UTCam = Cast<AUTPlayerCameraManager>(PlayerCameraManager);
		if (UTCam && UTCam->bIsForcingGoodCamLoc)
		{
			return true;
		}
		FName CameraStyle = (UTCam != NULL) ? UTCam->GetCameraStyleWithOverrides() : PlayerCameraManager->CameraStyle;
		return (CameraStyle == NAME_FreeCam) || (CameraStyle == NAME_RallyCam);
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
			if (UTChar != NULL)
			{
				UTChar->SetOutlineLocal(bTacComView);
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
	static const FName NAME_GameOver = FName(TEXT("GameOver"));
	ChangeState(NAME_GameOver);

	bool bIsInGameIntroHandlingEndGameSummary = false;
	if (GetWorld() && GetWorld()->GameState)
	{
		AUTGameState* UTGS = Cast<AUTGameState>(GetWorld()->GameState);
		if (UTGS)
		{
			if (UTGS->LineUpHelper && UTGS->LineUpHelper->bIsActive)
			{
				bIsInGameIntroHandlingEndGameSummary = UTGS->LineUpHelper->bIsActive;
			}
		}
	}

	if (!bIsInGameIntroHandlingEndGameSummary)
	{
		FinalViewTarget = EndGameFocus;
	}

	BehindView(true);
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AUTPlayerController::ShowEndGameScoreboard, 10.f, false);
	Super::ClientGameEnded_Implementation(EndGameFocus, bIsWinner);

	TurnOffPawns();

	// try to pick good end rotation that won't leave camera clipping into player
	if (FinalViewTarget != nullptr)
	{
		for (int32 i = 0; i < 16; i++)
		{
			FRotator TestRot = ControlRotation + FRotator(0.0f, 22.5 * i, 0.0f);
			if (!GetWorld()->SweepTestByChannel(FinalViewTarget->GetActorLocation(), FinalViewTarget->GetActorLocation() - TestRot.Vector() * 200.0f, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(FinalViewTarget->GetSimpleCollisionRadius())))
			{
				UE_LOG(UT, Log, TEXT("TEST: %s - %s"), *ControlRotation.ToString(), *TestRot.ToString());
				SetControlRotation(TestRot);
				break;
			}
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

void AUTPlayerController::ClientBackendNotify_Implementation(const FString& TypeStr, const FString& Data)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		TSharedPtr<FJsonValue> JsonData;
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(Data);
		if (FJsonSerializer::Deserialize(JsonReader, JsonData))
		{
			LP->HandleProfileNotification(FOnlineNotification(TypeStr, JsonData));
		}
	}
}

/*void AUTPlayerController::ClientReceiveXP_Implementation(FXPBreakdown GainedXP)
{
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (LP != NULL && LP->IsEarningXP())
	{
		LP->AddOnlineXP(GainedXP.Total());
		LP->SaveProfileSettings();

		//Store the XPBreakdown for the SUTXPBar
		XPBreakdown = GainedXP;
	}
}

void AUTPlayerController::ClientReceiveLevelReward_Implementation(int32 Level, const UUTProfileItem* RewardItem)
{
	// Store the reward. The SUTXPBar will display the toast when it triggers a level up
	LevelRewards.SetNumZeroed(FMath::Max<int32>(LevelRewards.Num(), Level + 1));
	LevelRewards[Level] = RewardItem;
}*/

void AUTPlayerController::ShowMenu(const FString& Parameters)
{
	ToggleScoreboard(false);
	Super::ShowMenu(Parameters);
	OnStopFire();
	OnStopAltFire();
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
	
	// Cancel this if we have an active line up, and go through the line up code to end up setting view target
	if (GetWorld())
	{
		AUTGameState* UTGS = Cast<AUTGameState>(GetWorld()->GetGameState());
		if (UTGS && UTGS->LineUpHelper && UTGS->LineUpHelper->bIsActive && (NewViewTarget != AUTLineUpHelper::GetCameraActorForLineUp(GetWorld(), UTGS->LineUpHelper->LastActiveType)))
		{
			ClientSetActiveLineUp(true, UTGS->LineUpHelper->LastActiveType);
			return;
		}
	}
	
	AActor* OldViewTarget = GetViewTarget();
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
			ViewProjectileTime = 0.f;
			LastSpectatedPlayerState = Cast<AUTPlayerState>(Char->PlayerState);
			if (LastSpectatedPlayerState)
			{
				LastSpectatedPlayerId = LastSpectatedPlayerState->SpectatingID;
			}
		}
		else if (!Cast<AUTProjectile>(UpdatedViewTarget) && (UpdatedViewTarget != this))
		{
			LastSpectatedPlayerState = NULL;
			LastSpectatedPlayerId = -1;
		}

		// FIXME: HACK: PlayerState->bOnlySpectator check is workaround for bug possessing new Pawn where we are actually in the spectating state for a short time after getting the new pawn as viewtarget
		//				happens because Pawn is replicated via property replication and ViewTarget is RPC'ed so comes first
		if (IsLocalController() && bSpectateBehindView && PlayerState && PlayerState->bOnlySpectator && (NewViewTarget != GetSpectatorPawn()) && NewViewTarget)
		{
			// pick good starting rotation
			FindGoodView(NewViewTarget->GetActorLocation(), false);
		}
	}
	if (MyUTHUD && (UpdatedViewTarget != OldViewTarget))
	{
		MyUTHUD->ClearIndicators();
		if (StateName == NAME_Spectating)
		{
			MyUTHUD->LastKillTime = -100.f;
		}
	}
}

FRotator AUTPlayerController::GetSpectatingRotation(const FVector& ViewLoc, float DeltaTime)
{
	if (IsInState(NAME_Spectating))
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS && (!GS->IsMatchInProgress() || GS->IsMatchIntermission()))
		{
			return GetControlRotation();
		}
		float OldYaw = FMath::UnwindDegrees(GetControlRotation().Yaw);
		FindGoodView(ViewLoc, true);
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

float AUTPlayerController::GetFrozenTime()
{
	return GetWorldTimerManager().GetTimerElapsed(TimerHandle_UnFreeze);
}

void AUTPlayerController::FindGoodView(const FVector& TargetLoc, bool bIsUpdate)
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
	BestRot.Yaw = FMath::UnwindDegrees(TestViewTarget->GetActorRotation().Yaw) + 15.f;
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
	}
	AUTPlayerCameraManager* CamMgr = Cast<AUTPlayerCameraManager>(PlayerCameraManager);
	static const FName NAME_GameOver = FName(TEXT("GameOver"));
	bool bGameOver = (GetStateName() == NAME_GameOver);
	float CameraDistance = bGameOver ? CamMgr->EndGameFreeCamDistance : CamMgr->FreeCamDistance;
	FVector CameraOffset = bGameOver ? CamMgr->EndGameFreeCamOffset : CamMgr->FreeCamOffset;
	float UnBlockedPct = (Cast<APawn>(TestViewTarget) && (CameraDistance > 0.f)) ? 96.f / CameraDistance : 1.f;

	AUTCTFFlag* Flag = Cast<AUTCTFFlag>(TestViewTarget);
	if (Flag)
	{
		if (Flag->IsHome() && Flag->HomeBase)
		{
			BestRot.Yaw = Flag->HomeBase->BestViewYaw;
		}
		else if (Flag->Holder && Flag->GetAttachmentReplication().AttachParent)
		{
			UnBlockedPct = (CameraDistance > 0.f) ? 96.f / CameraDistance : 1.f;
			BestRot.Yaw = Flag->GetAttachmentReplication().AttachParent->GetActorRotation().Yaw + 15.f;
		}
	}

	if ((TestViewTarget == FinalViewTarget) && Cast<AUTCharacter>(TestViewTarget))
	{
		UnBlockedPct = 1.f;
		BestRot.Yaw += 180.f;
	}

	// look for acceptable view
	float YawIncrement = 30.f;
	BestRot.Yaw = int32(BestRot.Yaw / 5.f) * 5.f;
	BestRot.Yaw = FMath::UnwindDegrees(BestRot.Yaw);
	if ((FMath::Abs(LastGoalYaw - BestRot.Yaw) < 6.f) || (FMath::Abs(LastGoalYaw - BestRot.Yaw) > 354.f))
	{
		// prevent jitter when can't settle on good view
		BestRot.Yaw = LastGoalYaw;
	}
	float OffsetMag = -60.f;
	float YawOffset = 0.f;
	bool bFoundGoodView = false;
	float BestView = BestRot.Yaw;
	float BestDist = 0.f;
	float StartYaw = BestRot.Yaw;
	int32 IncrementCount = 1;
	while (!bFoundGoodView && (IncrementCount < 12) && CamMgr)
	{
		BestRot.Yaw = StartYaw + YawOffset;
		FVector Pos = TargetLoc + FRotationMatrix(BestRot).TransformVector(CameraOffset) - BestRot.Vector() * CameraDistance;
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
		BestRot.Yaw = CurrentYaw;
	}
	LastGoalYaw = BestRot.Yaw;
	if ((FMath::Abs(CurrentYaw - BestRot.Yaw) > 60.f) && (FMath::Abs(CurrentYaw - BestRot.Yaw) < 300.f))
	{
		if (BestRot.Yaw < 0.f)
		{
			BestRot.Yaw += 360.f;
		}
		if (CurrentYaw < 0.f)
		{
			CurrentYaw += 360.f;
		}
		if (CurrentYaw > BestRot.Yaw)
		{
			if (360.f - CurrentYaw + BestRot.Yaw < CurrentYaw - BestRot.Yaw)
			{
				BestRot.Yaw = CurrentYaw + 30.f;
			}
			else
			{
				BestRot.Yaw = CurrentYaw - 30.f;
			}
		}
		else
		{
			if (360.f - BestRot.Yaw + CurrentYaw < BestRot.Yaw - CurrentYaw)
			{
				BestRot.Yaw = CurrentYaw - 30.f;
			}
			else
			{
				BestRot.Yaw = CurrentYaw + 30.f;
			}
		}
	}
	SetControlRotation(BestRot);
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
		AActor* NewViewTarget = GetSpectatorPawn() && !GetSpectatorPawn()->IsPendingKillPending() ? GetSpectatorPawn() : SpawnSpectatorPawn();
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
	// Freeze all of the pawns, destroy torn off ones
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		APawn* TestPawn = *It;
		if (TestPawn && !Cast<ASpectatorPawn>(TestPawn))
		{
			if (TestPawn->bTearOff)
			{
				TestPawn->Destroy();
			}
			else
			{
				TestPawn->TurnOff();
			}
		}
	}
	if (UTCharacter)
	{
		UTCharacter->SetAmbientSound(NULL);
		UTCharacter->SetLocalAmbientSound(NULL);
		UTCharacter->SetStatusAmbientSound(NULL);
	}
}

void AUTPlayerController::TestResult(uint16 ButtonID)
{
}

void AUTPlayerController::Possess(APawn* PawnToPossess)
{
	Super::Possess(PawnToPossess);

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
	// clear any victim message being displayed
	ClientReceiveLocalizedMessage(UUTVictimMessage::StaticClass(), 2, NULL, NULL, NULL);
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
	if (UTPlayerState)
	{
		UTPlayerState->CalculatePing(GetWorld()->GetTimeSeconds() - TimeStamp);
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
	FRotator CurrentRotation = GetControlRotation();
	Super::PlayerTick(DeltaTime);
	static const FName NAME_GameOver = FName(TEXT("GameOver"));
	if (StateName == NAME_GameOver)
	{
		UpdateRotation(DeltaTime);
	}
	else if (IsInState(NAME_Inactive))
	{
		AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
		if (GameState && !GameState->HasMatchEnded() && !GameState->IsMatchIntermission())
		{
			// revert any rotation changes
			SetControlRotation(CurrentRotation);
		}
	}

	// if we have no UTCharacterMovement, we need to apply firing here since it won't happen from the component
	if (GetPawn() == NULL || Cast<UUTCharacterMovement>(GetPawn()->GetMovementComponent()) == NULL)
	{
		ApplyDeferredFireInputs();
	}

	// Force ping update if servermoves aren't triggering it.
	if ((GetWorld()->GetTimeSeconds() - LastPingCalcTime > 0.5f) && (GetNetMode() == NM_Client))
	{
		LastPingCalcTime = GetWorld()->GetTimeSeconds();
		ServerBouncePing(GetWorld()->GetTimeSeconds());
	}
	APawn* ViewTargetPawn = PlayerCameraManager->GetViewTargetPawn();
	AUTCharacter* ViewTargetCharacter = Cast<AUTCharacter>(ViewTargetPawn);
	if (IsInState(NAME_Spectating) && UTPlayerState  && bAutoCam && (UTPlayerState->bOnlySpectator || (UTPlayerState->bOutOfLives && !Cast<AUTGameObjective>(GetViewTarget()))) && (!ViewTargetCharacter || !ViewTargetCharacter->IsRecentlyDead()))
	{
		// possibly switch cameras
		ChooseBestCamera();
	}

	// Follow the last spectated player again when they respawn
	if ((StateName == NAME_Spectating) && LastSpectatedPlayerId >= 0 && IsLocalController() && (!Cast<AUTProjectile>(GetViewTarget()) || GetViewTarget()->IsPendingKillPending()))
	{
		ViewTargetPawn = PlayerCameraManager->GetViewTargetPawn();
		ViewTargetCharacter = Cast<AUTCharacter>(ViewTargetPawn);
		if (!ViewTargetPawn || ViewTargetPawn->IsPendingKillPending() || (ViewTargetCharacter && ViewTargetCharacter->IsDead() && !ViewTargetCharacter->IsRecentlyDead()))
		{
			for (FConstPawnIterator Iterator = GetWorld()->GetPawnIterator(); Iterator; ++Iterator)
			{
				APawn* PawnIter = *Iterator;
				if (PawnIter != nullptr)
				{
					AUTPlayerState* PS = Cast<AUTPlayerState>(PawnIter->PlayerState);
					if (PS && PS->SpectatingID == LastSpectatedPlayerId)
					{
						AUTCharacter* TargetChar = Cast<AUTCharacter>(PawnIter);
						if (TargetChar && TargetChar->DrivenVehicle && !TargetChar->DrivenVehicle->IsPendingKillPending())
						{
							ViewPawn(TargetChar->DrivenVehicle);
						}
						else
						{
							ViewPawn(*Iterator);
						}
						break;
					}
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

	// Prestream everyone's textures for match summary
	if (GetNetMode() != NM_DedicatedServer)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS && GS->bPlayPlayerIntro && GS->GetMatchState() == MatchState::WaitingToStart)
		{
			for (TActorIterator<AUTPlayerState> It(GetWorld()); It; ++It)
			{
				AUTPlayerState* PS = *It;
				if (!PS->bOnlySpectator && !PS->IsPendingKillPending())
				{
					TSubclassOf<AUTCharacterContent> Data = PS->GetSelectedCharacter();
					if (Data)
					{
						USkeletalMeshComponent* SkelMesh = Data->GetDefaultObject<AUTCharacterContent>()->GetMesh();
						if (SkelMesh)
						{
							SkelMesh->PrestreamTextures(1, true);
						}
					}
				}
			}
		}
	}

	if ((GetNetMode() == NM_DedicatedServer) && (CurrentlyViewedScorePS != NULL))
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS)
		{
			if ((GS->Teams.Num() > TeamStatsUpdateTeam) && (GS->Teams[TeamStatsUpdateTeam] != NULL))
			{
				if ((TeamStatsUpdateIndex == 0) && (TeamStatsUpdateTeam == 0))
				{
					LastTeamStatsUpdateStartTime = GetWorld()->GetTimeSeconds();
				}
				if (TeamStatsUpdateIndex < GS->TeamStats.Num())
				{
					ClientUpdateTeamStats(TeamStatsUpdateTeam, TeamStatsUpdateIndex, GS->Teams[TeamStatsUpdateTeam]->GetStatsValue(GS->TeamStats[TeamStatsUpdateIndex]));
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
			else if (CurrentlyViewedStatsTab == 3)
			{
				StatArraySize = GS->MovementStats.Num();
				if (StatsUpdateIndex < StatArraySize)
				{
					StatsName = GS->MovementStats[StatsUpdateIndex];
				}
			}
			if (StatsUpdateIndex < StatArraySize)
			{
				ClientUpdateScoreStats(CurrentlyViewedScorePS, CurrentlyViewedStatsTab, StatsUpdateIndex, CurrentlyViewedScorePS->GetStatsValue(StatsName));
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

	if (GetWorld()->GetTimeSeconds() < ViewProjectileTime)
	{
		ServerViewProjectile_Implementation();
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
			AUTCharacter* CamPawn = Cast<AUTCharacter>(*Iterator);
			AUTPlayerState* NextPlayerState = (CamPawn && (CamPawn->Health > 0)) ? Cast<AUTPlayerState>(CamPawn->PlayerState) : NULL;
			if (NextPlayerState)
			{
				float NewScore = UTCam->RatePlayerCamera(NextPlayerState, CamPawn, LastSpectatedPlayerState);
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
	FVector ShotDir(FVector::ZeroVector);
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		ShotDir = ((FPointDamageEvent*)&DamageEvent)->ShotDirection;
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID) && ((FRadialDamageEvent*)&DamageEvent)->ComponentHits.Num() > 0)
	{
		if (DamageEvent.IsOfType(FUTRadialDamageEvent::ClassID) && (((FUTRadialDamageEvent*)&DamageEvent)->Params.MinimumDamage == ((FUTRadialDamageEvent*)&DamageEvent)->Params.BaseDamage))
		{
			ShotDir = ((FUTRadialDamageEvent*)&DamageEvent)->ShotDirection;
		}
		else
		{
		ShotDir = (((FRadialDamageEvent*)&DamageEvent)->ComponentHits[0].ImpactPoint - ((FRadialDamageEvent*)&DamageEvent)->Origin).GetSafeNormal();
	}
	}
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	bool bFriendlyFire = InstigatedByState != PlayerState && GS != NULL && GS->OnSameTeam(InstigatedByState, this);
	uint8 RepDamage = FMath::Clamp(Damage, 0, 255);
	ClientNotifyTakeHit(bFriendlyFire, RepDamage, FRotator::CompressAxisToByte(ShotDir.Rotation().Yaw), DamageEvent.DamageTypeClass);
}

void AUTPlayerController::ClientNotifyTakeHit_Implementation(bool bFriendlyFire, uint8 Damage, uint8 ShotDirYaw, TSubclassOf<class UDamageType> DamageTypeClass)
{
	if (MyUTHUD != NULL)
	{
		MyUTHUD->PawnDamaged(ShotDirYaw, Damage, bFriendlyFire, DamageTypeClass);
	}
}

void AUTPlayerController::ClientNotifyCausedHit_Implementation(APawn* HitPawn, uint8 Damage)
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

void AUTPlayerController::ChangeTeam(uint8 NewTeamIndex)
{
	if (UTPlayerState != NULL)
	{
		UTPlayerState->ServerRequestChangeTeam(NewTeamIndex);
	}
}

void AUTPlayerController::Suicide()
{
	ServerSuicide();
}

void AUTPlayerController::ServerSuicide_Implementation()
{
	// throttle suicides to avoid spamming to grief own team in TDM
	AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (!Game || Game->AllowSuicideBy(this))
	{
		AUTCharacter* Char = Cast<AUTCharacter>(GetPawn());
		if (Char != NULL)
		{
			Char->PlayerSuicide();
		}
		else
		{
			AUTRemoteRedeemer* Deemer = Cast <AUTRemoteRedeemer>(GetPawn());
			Char = Deemer ? Cast<AUTCharacter>(Deemer->Driver) : nullptr;
			if (Char != NULL)
			{
				Char->PlayerSuicide();
			}
		}
	}
	else if ((UTCharacter == nullptr) && UTPlayerState && (UTPlayerState->ReadyMode == 4))
	{
		UTPlayerState->ReadyMode = 3;
	}
}

bool AUTPlayerController::ServerSuicide_Validate()
{
	return true;
}

void AUTPlayerController::SetWeaponHand(EWeaponHand NewHand)
{
	ReplicatedWeaponHand = NewHand;

	AUTCharacter* UTCharTarget = Cast<AUTCharacter>(GetViewTarget());
	if (UTCharTarget != NULL && UTCharTarget->GetWeapon() != NULL)
	{
		UTCharTarget->GetWeapon()->UpdateWeaponHand();
	}
	if (IsTemplate() || IsLocalPlayerController())
	{
		SaveConfig();
	}
	if (!IsTemplate() && Role < ROLE_Authority)
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
	SetWeaponHand(NewHand);
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
	if ((UTCharacter == nullptr) && UTPlayerState && (UTPlayerState->ReadyMode == 3))
	{
		UTPlayerState->ReadyMode = 5;
	}
	else if (UTPlayerState != nullptr)
	{
		UTPlayerState->PlayTauntByIndex(EmoteIndex);
	}
}

void AUTPlayerController::PlayGroupTaunt()
{
	if (GetWorld()->GetRealTimeSeconds() - LastEmoteTime > EmoteCooldownTime)
	{
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS && GS->ScoringPlayerState == PlayerState && (GS->IsMatchIntermission() || GS->HasMatchEnded()))
		{
			ServerPlayGroupTaunt();
			LastEmoteTime = GetWorld()->GetRealTimeSeconds();
		}
	}
}

bool AUTPlayerController::ServerPlayGroupTaunt_Validate()
{
	return true;
}

void AUTPlayerController::ServerPlayGroupTaunt_Implementation()
{
	if (UTPlayerState != nullptr)
	{
		UTPlayerState->PlayGroupTaunt();
	}
}

//Special markup for Analytics event so they show up properly in grafana. Should be eventually moved to UTAnalytics.
/*
* @EventName PlayerConnect
*
* @Trigger Sent when the client or server receives a player
*
* @Type Sent by the Client and Server
*
* @Comments
*/
void AUTPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	
	UUTLocalPlayer* LP = Cast<UUTLocalPlayer>(Player);
	if (LP != NULL)
	{
		if (GetNetMode() != NM_Standalone)
		{
			ServerSetWeaponHand(GetWeaponHand());
			if (FUTAnalytics::IsAvailable() && (GetWorld()->GetNetMode() != NM_Client || GetWorld()->GetNetDriver() != NULL)) // make sure we don't do analytics for demo playback
			{
				FString ServerInfo = (GetWorld()->GetNetMode() == NM_Client) ? GetWorld()->GetNetDriver()->ServerConnection->URL.ToString() : GEngine->GetWorldContextFromWorldChecked(GetWorld()).LastURL.ToString();
				FUTAnalytics::GetProvider().RecordEvent(TEXT("PlayerConnect"), TEXT("Server"), ServerInfo);
			}
		}

		// Send over the country flag....
		UUTProfileSettings* Settings = GetProfileSettings();
		if (Settings != NULL)
		{
			FName CountryFlag = Settings->CountryFlag;

			UUTGameEngine* UTEngine = Cast<UUTGameEngine>(GEngine);
			if (UTEngine != nullptr)
			{
				if (CountryFlag == NAME_None)
				{
					// see if I am entitled to Epic flag, if so use it as default
					CountryFlag = NAME_Epic;
				}
				UUTFlagInfo* Flag = UTEngine->GetFlag(CountryFlag);
				if (Flag == nullptr || !Flag->IsEntitled(LP->CommunityRole))
				{
					CountryFlag = NAME_None;
				}
			}
			ServerReceiveCountryFlag(CountryFlag);
		}
	}

	if (GetNetMode() == NM_Client || GetNetMode() == NM_Standalone)
	{
		InitializeHeartbeatManager();
	}
}

bool AUTPlayerController::ServerReceiveCountryFlag_Validate(FName NewCountryFlag)
{
	return true;
}

//Special markup for Analytics event so they show up properly in grafana. Should be eventually moved to UTAnalytics.
/*
* @EventName FlagChange
*
* @Trigger Sent when a user changes their country flag
*
* @Type Sent by the Client
*
* @EventParam CountryFlag string New Country
* @EventParam UserId string UniqueId of the user that changed their flag
*
* @Comments
*/
void AUTPlayerController::ServerReceiveCountryFlag_Implementation(FName NewCountryFlag)
{
	if (UTPlayerState != NULL)
	{
		UTPlayerState->CountryFlag = NewCountryFlag;

		if (FUTAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("CountryFlag"), UTPlayerState->CountryFlag.ToString()));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("UserId"), UTPlayerState->UniqueId.ToString()));
			FUTAnalytics::GetProvider().RecordEvent(TEXT("FlagChange"), ParamArray);
		}
	}
}

void AUTPlayerController::ApplyDeferredFireInputs()
{
	for (FDeferredFireInput& Input : DeferredFireInputs)
	{
		if (Input.bStartFire)
		{
			if (!IsMoveInputIgnored())
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
	if (UTPlayerState != nullptr)
	{
		UTPlayerState->ServerSetEmoteSpeed(NewEmoteSpeed);
	}
}

void AUTPlayerController::FasterEmote()
{
	if (UTPlayerState != nullptr && UTCharacter != nullptr && UTCharacter->CurrentTaunt != nullptr)
	{
		UTPlayerState->ServerFasterEmote();
	}
}

void AUTPlayerController::SlowerEmote()
{
	if (UTPlayerState != nullptr && UTCharacter != nullptr && UTCharacter->CurrentTaunt != nullptr)
	{
		UTPlayerState->ServerSlowerEmote();
	}
}

void AUTPlayerController::ViewPawn(APawn* PawnToView)
{
	ServerViewPawn(PawnToView);
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

void AUTPlayerController::SetGamepadSensitivityLeft(float NewSensitivity)
{
	FInputAxisProperties AxisProps;
	if (PlayerInput->GetAxisProperties(EKeys::Gamepad_LeftX, AxisProps))
	{
		AxisProps.Sensitivity = NewSensitivity;
		PlayerInput->SetAxisProperties(EKeys::Gamepad_LeftX, AxisProps);
	}
	if (PlayerInput->GetAxisProperties(EKeys::Gamepad_LeftY, AxisProps))
	{
		AxisProps.Sensitivity = NewSensitivity;
		PlayerInput->SetAxisProperties(EKeys::Gamepad_LeftY, AxisProps);
	}

	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::Gamepad_LeftX || Entry.AxisKeyName == EKeys::Gamepad_LeftY)
		{
			Entry.AxisProperties.Sensitivity = NewSensitivity;
		}
	}
	InputSettings->SaveConfig();
	
	if (Cast<UUTLocalPlayer>(Player))
	{
		Cast<UUTLocalPlayer>(Player)->SaveProfileSettings();
	}
}

void AUTPlayerController::SetGamepadSensitivityRight(float NewSensitivity)
{
	FInputAxisProperties AxisProps;
	if (PlayerInput->GetAxisProperties(EKeys::Gamepad_RightX, AxisProps))
	{
		AxisProps.Sensitivity = NewSensitivity;
		PlayerInput->SetAxisProperties(EKeys::Gamepad_RightX, AxisProps);
	}
	if (PlayerInput->GetAxisProperties(EKeys::Gamepad_RightY, AxisProps))
	{
		AxisProps.Sensitivity = NewSensitivity;
		PlayerInput->SetAxisProperties(EKeys::Gamepad_RightY, AxisProps);
	}

	UInputSettings* InputSettings = UInputSettings::StaticClass()->GetDefaultObject<UInputSettings>();
	for (FInputAxisConfigEntry& Entry : InputSettings->AxisConfig)
	{
		if (Entry.AxisKeyName == EKeys::Gamepad_RightX || Entry.AxisKeyName == EKeys::Gamepad_RightY)
		{
			Entry.AxisProperties.Sensitivity = NewSensitivity;
		}
	}
	InputSettings->SaveConfig();

	if (Cast<UUTLocalPlayer>(Player))
	{
		Cast<UUTLocalPlayer>(Player)->SaveProfileSettings();
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

float AUTPlayerController::GetWeaponAutoSwitchPriority(AUTWeapon* InWeapon)
{
	if (Cast<UUTLocalPlayer>(Player))
	{
		UUTProfileSettings* ProfileSettings = GetProfileSettings();
		if (ProfileSettings)
		{
			FWeaponCustomizationInfo WeaponCustomization;
			ProfileSettings->GetWeaponCustomizationForWeapon(InWeapon, WeaponCustomization);
			return WeaponCustomization.WeaponAutoSwitchPriority;
		}
	}
	return 0.0f;
}

int32 AUTPlayerController::GetWeaponGroup(AUTWeapon* InWeapon)
{
	if (Cast<UUTLocalPlayer>(Player) && InWeapon != nullptr)
	{
		UUTProfileSettings* ProfileSettings = GetProfileSettings();
		if (ProfileSettings)
		{
			FWeaponCustomizationInfo WeaponCustomization;
			ProfileSettings->GetWeaponCustomizationForWeapon(InWeapon, WeaponCustomization);
			return WeaponCustomization.WeaponGroup;
		}
	}

	return 1.0f;
}

void AUTPlayerController::ClientSay_Implementation(AUTPlayerState* Speaker, const FString& Message, FName Destination)
{
	UUTGameUserSettings* GS = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (Speaker == NULL || !Speaker->bIsABot || GS == NULL || GS->GetBotSpeech() > BSO_None)
	{
		UTClientPlaySound(ChatMsgSound);
		Super::ClientSay_Implementation(Speaker, Message, Destination);
	}
}

void AUTPlayerController::UTClientPlaySound_Implementation(USoundBase* Sound)
{
	UUTGameplayStatics::UTPlaySound(GetWorld(), Sound, this, SRT_None, true, FVector::ZeroVector, nullptr, nullptr, false, SAT_None);
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
	if ((GetPawn() != NULL) && (GetPawn()->Controller == this))
	{
		GetPawn()->Controller = NULL;
	}
	SetPawn(NULL);

	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	float const MinRespawnDelay = GameState ? GameState->GetRespawnWaitTimeFor(UTPlayerState) : 1.5f;
	GetWorldTimerManager().SetTimer(TimerHandle_UnFreeze, this, &APlayerController::UnFreeze, MinRespawnDelay);

	if (GameState && MyUTHUD && GameState->IsMatchInProgress() && !GameState->IsMatchIntermission() && (TSubclassOf<AUTGameMode>(*GameState->GameModeClass) == nullptr || !TSubclassOf<AUTGameMode>(*GameState->GameModeClass).GetDefaultObject()->bHasRespawnChoices))
	{
		GetWorldTimerManager().SetTimer(TImerHandle_ShowScoreboardOnDeath, this, &AUTPlayerController::ShowScoreboardOnDeath, ScoreboardDelayOnDeath);
	}
}

void AUTPlayerController::ShowScoreboardOnDeath()
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (!GetPawn() && GameState && MyUTHUD && GameState->IsMatchInProgress() && !GameState->IsMatchIntermission() && !IsInState(NAME_Spectating))
	{
		MyUTHUD->bShowScoresWhileDead = true;
	}
}

void AUTPlayerController::ServerViewPlaceholderAtLocation_Implementation(FVector Location)
{
	if (GetPawn() == NULL && (IsInState(NAME_Spectating) || IsInState(NAME_Inactive)))
	{
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
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


	OutputFile.Logf(TEXT("Dumping BugIt data chart at %s using build %s built from changelist %i"), *FDateTime::Now().ToString(), *FEngineVersion::Current().ToString(), GetChangeListNumberForPerfTesting());

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
				int32 InsertIndex = LP->GetGameInstance()->AddLocalPlayer(NewPlayer, 7);
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
	bAutoCam = false;
	if (CastingGuideStartupCommands.IsValidIndex(CastingGuideViewIndex) && !CastingGuideStartupCommands[CastingGuideViewIndex].IsEmpty())
	{
		ConsoleCommand(CastingGuideStartupCommands[CastingGuideViewIndex]);
	}
}

void AUTPlayerController::StartCastingGuide()
{
	if (!bCastingGuide && Role < ROLE_Authority && PlayerState != NULL && PlayerState->bOnlySpectator)
	{
		ServerStartCastingGuide();
	}
}
bool AUTPlayerController::ServerStartCastingGuide_Validate()
{
	return true;
}
void AUTPlayerController::ServerStartCastingGuide_Implementation()
{
	if (!bCastingGuide && Role == ROLE_Authority && Cast<UNetConnection>(Player) != NULL && Cast<UChildConnection>(Player) == NULL && PlayerState != NULL && PlayerState->bOnlySpectator)
	{
		AUTGameMode* Game = GetWorld()->GetAuthGameMode<AUTGameMode>();
		if (Game != NULL)
		{
			Game->SwitchToCastingGuide(this);
		}
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

	UUTProfileSettings* ProfileSettings = GetProfileSettings();
	if (ProfileSettings)
	{
		const FKeyConfigurationInfo* GameAction = ProfileSettings->FindGameAction(Command);
		if (GameAction)
		{
			if (GameAction->PrimaryKey != FKey()) Keys.Add(GameAction->PrimaryKey);
			if (GameAction->SecondaryKey != FKey()) Keys.Add(GameAction->SecondaryKey);
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

void AUTPlayerController::SkullPickedUp()
{
	// deprecated
}

void AUTPlayerController::PumpkinPickedUp(float GainedAmount, float GoalAmount)
{
	ClientPumpkinPickedUp(GainedAmount, GoalAmount);
}

void AUTPlayerController::ClientPumpkinPickedUp_Implementation(float GainedAmount, float GoalAmount)
{
	static FName FacePumpkins(TEXT("FacePumpkins"));
	float TotalPumpkins = 0.0f;
	UUTGameplayStatics::GetBestTime(GetWorld(), FacePumpkins, TotalPumpkins);
	TotalPumpkins += GainedAmount;
	TotalPumpkins = FMath::Min(GoalAmount, TotalPumpkins);
	UUTGameplayStatics::SetBestTime(GetWorld(), FacePumpkins, TotalPumpkins);
}

void AUTPlayerController::DebugTest(FString TestCommand)
{
	if (UTCharacter != nullptr)
	{
		AUTGauntletFlag* Flag = Cast<AUTGauntletFlag>(UTCharacter->GetCarriedObject());
		if (Flag != nullptr)
		{
			if (TestCommand.Equals(TEXT("debugon"), ESearchCase::IgnoreCase))
			{
				Flag->bDebugGPS = true;
			}
			else if (TestCommand.Equals(TEXT("debugoff"), ESearchCase::IgnoreCase))
			{
				Flag->bDebugGPS = false;
				FlushPersistentDebugLines(GetWorld());
				FlushDebugStrings(GetWorld());
			}
			else if (TestCommand.Equals(TEXT("gpson"), ESearchCase::IgnoreCase))
			{
				Flag->bDisableGPS = false;
			}
			else if (TestCommand.Equals(TEXT("gpsoff"), ESearchCase::IgnoreCase))
			{
				Flag->bDisableGPS = true;
			}
		}
	}

	Super::DebugTest(TestCommand);

}

void AUTPlayerController::ServerDebugTest_Implementation(const FString& TestCommand)
{
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

void AUTPlayerController::TurnOffPawns()
{
	// freeze all Pawns locally
	for (FConstPawnIterator It = GetWorld()->GetPawnIterator(); It; ++It)
	{
		if (It->IsValid() && !Cast<ASpectatorPawn>(It->Get()))
		{
			It->Get()->TurnOff();
		}
	}
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
	
	//Let the UTWeapons know that any HUD text needs to be updated
	if (GetUTCharacter())
	{
		for (TInventoryIterator<AUTWeapon> It(GetUTCharacter()); It; ++It)
		{
			It->UpdateHUDText();
		}
	}
}

void AUTPlayerController::UpdateInventoryKeys()
{
	//Let the UT Inventory Items know that any HUD text needs to be updated
	if (GetUTCharacter())
	{
		for (TInventoryIterator<AUTInventory> It(GetUTCharacter()); It; ++It)
		{
			It->UpdateHUDText();
		}
	}
}

bool AUTPlayerController::ServerRegisterBanVote_Validate(AUTPlayerState* BadGuy) { return true; }
void AUTPlayerController::ServerRegisterBanVote_Implementation(AUTPlayerState* BadGuy)
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState && UTPlayerState && BadGuy && !UTPlayerState->bOnlySpectator)
	{
		GameState->VoteForTempBan(BadGuy, UTPlayerState);
	}
}

FRotator AUTPlayerController::GetControlRotation() const
{
	if (UTPlayerState && (UTPlayerState->bOnlySpectator || UTPlayerState->bOutOfLives) && !IsBehindView() && (GetViewTarget() != GetSpectatorPawn()) && (GetViewTarget() != GetPawn()))
	{
		return BlendedTargetViewRotation;
	}
	ControlRotation.DiagnosticCheckNaN();
	return ControlRotation;
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
	//Prevents the menu from opening/closing by rapid inputs
	if (((GetWorld()->GetTimeSeconds() - LastBuyMenuOpenTime) >= BuyMenuToggleDelay) || (LastBuyMenuOpenTime <= SMALL_NUMBER))
	{
		LastBuyMenuOpenTime = GetWorld()->GetTimeSeconds();

		// It's silly to send this to the server before handling it here.  I probably should just for safe keepeing but for now
		// just locally.

		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS && GS->AvailableLoadout.Num() > 0)
		{
			if (GetPawn() == nullptr || !GS->HasMatchStarted() || GS->IsMatchIntermission())
			{
				ClientOpenLoadout_Implementation(true);
			}
		}
	}
}

void AUTPlayerController::DropCarriedObject()
{
	if (UTCharacter && !UTCharacter->IsFiringDisabled())
	{
		UTCharacter->DropCarriedObject();
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

void AUTPlayerController::ClientUpdateScoreStats_Implementation(AUTPlayerState* ViewedPS, uint8 StatsPage, uint8 StatsIndex, float NewValue)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (ViewedPS && GS)
	{
		FName StatsName = NAME_None;
		if (StatsPage == 0)
		{
			if (StatsUpdateIndex < GS->GameScoreStats.Num())
			{
				StatsName = GS->GameScoreStats[StatsIndex];
			}
		}
		else if (StatsPage == 1)
		{
			if (StatsUpdateIndex < GS->WeaponStats.Num())
			{
				StatsName = GS->WeaponStats[StatsIndex];
			}
		}
		else if (StatsPage == 2)
		{
			if (StatsUpdateIndex < GS->RewardStats.Num())
			{
				StatsName = GS->RewardStats[StatsIndex];
			}
		}
		else if (StatsPage == 3)
		{
			if (StatsUpdateIndex < GS->MovementStats.Num())
			{
				StatsName = GS->MovementStats[StatsIndex];
			}
		}
		ViewedPS->SetStatsValue(StatsName, NewValue);
	}
}

void AUTPlayerController::ClientUpdateTeamStats_Implementation(uint8 TeamNum, uint8 TeamStatsIndex, float NewValue)
{
	AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
	if (GS && (GS->Teams.Num() > TeamNum) && GS->Teams[TeamNum])
	{
		FName StatsName = (GS->TeamStats.Num() > TeamStatsIndex) ? GS->TeamStats[TeamStatsIndex] : NAME_None;
		if (StatsName == NAME_None)
		{
			UE_LOG(UT, Warning, TEXT("Failed teamstats assignment index %d"), TeamStatsIndex);
		}
		GS->Teams[TeamNum]->SetStatsValue(StatsName, NewValue);
	}
}

void AUTPlayerController::ClientUpdateSkillRating_Implementation(const FString& MatchRatingType)
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->ReadSpecificELOFromBackend(MatchRatingType);
	}
}

void AUTPlayerController::ClientShowMapVote_Implementation()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (LocalPlayer != NULL && GameState != NULL)
	{
		LocalPlayer->OpenMapVote(GameState);
	}
}
void AUTPlayerController::ClientHideMapVote_Implementation()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		LocalPlayer->CloseMapVote();
	}
}

void AUTPlayerController::ClearTokens()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		UUTProgressionStorage* Storage = LocalPlayer->GetProgressionStorage();
		if (Storage != NULL)
		{
			Storage->TokensClear();
		}
	}
#endif
}

AUTCharacter* AUTPlayerController::GhostTrace()
{
	FVector CameraLoc;
	FRotator CameraRot;
	GetPlayerViewPoint(CameraLoc, CameraRot);
	FHitResult Hit;
	GetWorld()->LineTraceSingleByChannel(Hit, CameraLoc, CameraLoc + CameraRot.Vector() * 50000.0f, COLLISION_TRACE_WEAPON, FCollisionQueryParams(FName(TEXT("GhostTrace")), true, GetUTCharacter()));

	return Cast<AUTCharacter>(Hit.Actor.Get());
}

void AUTPlayerController::GhostStart()
{
	if (GetWorld()->WorldType == EWorldType::PIE && Role == ROLE_Authority)
	{
		AUTCharacter* HitChar = GhostTrace();
		if (HitChar != nullptr && (HitChar->GetController() == nullptr || Cast<AAIController>(HitChar->GetController()) != nullptr))
		{
			if (HitChar->GhostComponent->bGhostPlaying)
			{
				HitChar->GhostComponent->GhostStopPlaying();
			}
			//Store our original char so we can switch back later
			PreGhostChar = GetUTCharacter();

			Possess(HitChar);
			SetViewTarget(HitChar);
			HitChar->GhostComponent->GhostStartRecording();
		}
	}
}

void AUTPlayerController::GhostStop()
{
	if (GetWorld()->WorldType == EWorldType::PIE && Role == ROLE_Authority)
	{
		AUTCharacter* UTC = GetUTCharacter();
		if (UTC != nullptr && UTC->GhostComponent->bGhostRecording)
		{
			UTC->GhostComponent->GhostStopRecording();

			//
			if (PreGhostChar != nullptr)
			{
				Possess(PreGhostChar);
				SetViewTarget(PreGhostChar);
			}

			//Give it a controller and move it back to its original position
			UTC->SpawnDefaultController();
			UTC->GhostComponent->GhostMoveToStart();
		}
	}
}

void AUTPlayerController::GhostPlay()
{
	if (GetWorld()->WorldType == EWorldType::PIE)
	{
		AUTCharacter* UTC = GhostTrace();
		if (UTC != nullptr)
		{
			if (UTC->GhostComponent->bGhostPlaying)
			{
				UTC->GhostComponent->GhostStopPlaying();
			}
			UTC->GhostComponent->GhostStartPlaying();
		}
	}
}

void AUTPlayerController::OpenMatchSummary()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(Player);
	AUTGameState* UTGS = Cast<AUTGameState>(GetWorld()->GameState);
	if (LocalPlayer != nullptr && UTGS != nullptr)
	{
		LocalPlayer->OpenMatchSummary(UTGS);
	}
}

void AUTPlayerController::UTClientSetRotation_Implementation(FRotator NewRotation)
{
	SetControlRotation(NewRotation);
//	UE_LOG(UT, Warning, TEXT("Control Rotation from UTClientSetRotation %f"), GetControlRotation().Yaw);
	if (GetPawn() != NULL)
	{
		GetPawn()->FaceRotation(NewRotation, 0.f);
	}
}

void AUTPlayerController::ClientUpdateDamageDone_Implementation(int32 DamageDone, int32 RoundDamageDone)
{
	if (UTPlayerState)
	{
		UTPlayerState->DamageDone = DamageDone;
		UTPlayerState->RoundDamageDone = RoundDamageDone;
	}
}

void AUTPlayerController::ToggleShowDamage()
{
	if (MyUTHUD)
	{
		MyUTHUD->bDrawDamageNumbers = !MyUTHUD->bDrawDamageNumbers;
	}
}

#if WITH_PROFILE

UUtMcpProfileManager* AUTPlayerController::GetMcpProfileManager()
{
	UUTLocalPlayer *LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		return LocalPlayer->GetMcpProfileManager();
	}
	return nullptr;
}


UUtMcpProfileManager* AUTPlayerController::GetMcpProfileManager(const FString& AccountId)
{
	UUTLocalPlayer *LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		return LocalPlayer->GetMcpProfileManager(AccountId);
	}
	return nullptr;
}

UUtMcpProfileManager* AUTPlayerController::GetActiveMcpProfileManager()
{
	UUTLocalPlayer *LocalPlayer = Cast<UUTLocalPlayer>(Player);
	if (LocalPlayer)
	{
		return LocalPlayer->GetActiveMcpProfileManager();
	}
	return nullptr;
}

#endif

void AUTPlayerController::TestCallstack()
{
	ANSICHAR StackTrace[4096];
	if (StackTrace != NULL)
	{
		StackTrace[0] = 0;
		FPlatformStackWalk::StackWalkAndDump(StackTrace, ARRAY_COUNT(StackTrace), 2);
	}

	UE_LOG(UT, Log, TEXT("%s"), ANSI_TO_TCHAR(StackTrace));
}

void AUTPlayerController::ResetFoliageDitheredLOD()
{
	static auto DitheredLODCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("foliage.DitheredLOD"));
	DitheredLODCVar->Set(true, ECVF_SetByGameSetting);
}

void AUTPlayerController::ShowComsMenu()
{
	if (MyUTHUD) MyUTHUD->ToggleComsMenu(true);
}

void AUTPlayerController::HideComsMenu()
{
	if (MyUTHUD) MyUTHUD->ToggleComsMenu(false);
}

void AUTPlayerController::ShowWeaponWheel()
{
	if (MyUTHUD) MyUTHUD->ToggleWeaponWheel(true);
}

void AUTPlayerController::HideWeaponWheel()
{
	if (MyUTHUD) MyUTHUD->ToggleWeaponWheel(false);
}

void AUTPlayerController::FlushVOIP()
{
	UUTProfileSettings* ProfileSettings = GetProfileSettings();
	if (ProfileSettings && ProfileSettings->bPushToTalk)
	{
		IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(GetWorld());
		if (VoiceInt.IsValid())
		{
			VoiceInt->ClearVoicePackets();
			ToggleSpeaking(false);
			FTimerHandle TimerHandle;
			GetWorldTimerManager().SetTimer(TimerHandle, this, &AUTPlayerController::RestartVOIP, 0.5f, false);
		}
	}
}

void AUTPlayerController::RestartVOIP()
{
	ToggleSpeaking(true);
}

void AUTPlayerController::VoiceDebug(const FString& Command)
{
	ProcessVoiceDebug(Command);
	ServerVoiceDebug(Command);
}

bool AUTPlayerController::ServerVoiceDebug_Validate(const FString& Command) { return true; }
void AUTPlayerController::ServerVoiceDebug_Implementation(const FString& Command)
{
	ProcessVoiceDebug(Command);
}

void AUTPlayerController::ProcessVoiceDebug(const FString& Command)
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	IOnlineVoicePtr VoiceInt = Online::GetVoiceInterface(GetWorld());

	if ( MyUTHUD && Command.Equals(TEXT("show"), ESearchCase::IgnoreCase) )
	{
		MyUTHUD->bShowVoiceDebug = true;
		return;
	}

	if ( MyUTHUD && Command.Equals(TEXT("hide"), ESearchCase::IgnoreCase) )
	{
		MyUTHUD->bShowVoiceDebug = false;
		return;
	}

	if ( Command.Equals(TEXT("toggleon"), ESearchCase::IgnoreCase) && GetNetMode() == NM_Client)
	{
		ToggleSpeaking(true);
		return;
	}

	if ( Command.Equals(TEXT("toggleoff"), ESearchCase::IgnoreCase) && GetNetMode() == NM_Client )
	{
		ToggleSpeaking(false);
		return;
	}

	if ( Command.Equals(TEXT("clearvoice"), ESearchCase::IgnoreCase) )
	{
		if (VoiceInt.IsValid())
		{
			VoiceInt->ClearVoicePackets();
			return;
		}
	}

	if ( GameState && Command.Equals(TEXT("dump"), ESearchCase::IgnoreCase) )
	{
		UE_LOG(UT,Log,TEXT("[PlayerList]========================================================================="));
		for (int32 i=0; i < GameState->PlayerArray.Num(); i++)
		{
			if (GameState->PlayerArray[i])
			{
				UE_LOG(UT,Log,TEXT("Player %i %s = %s"),i, *GameState->PlayerArray[i]->PlayerName, *GameState->PlayerArray[i]->UniqueId.ToString());
			}
		}

		UE_LOG(UT,Log,TEXT("[Mute List]========================================================================="));

		FString Text = DumpMutelistState(GetWorld());
		TArray<FString> Lines;
		Text.ParseIntoArray(Lines,TEXT("\n"), false);

		for (int32 i=0; i < Lines.Num(); i++)
		{
			UE_LOG(UT,Log,TEXT("%s"), *Lines[i]);
		}
		
		if (VoiceInt.IsValid())
		{
			UE_LOG(UT,Log,TEXT("[Voice List]========================================================================="));

			Text = VoiceInt->GetVoiceDebugState();
			Lines.Empty();
			Text.ParseIntoArray(Lines,TEXT("\n"), false);

			for (int32 i=0; i < Lines.Num(); i++)
			{
				UE_LOG(UT,Log,TEXT("%s"), *Lines[i]);
			}
		}
		UE_LOG(UT,Log,TEXT("====================================================================================="));
	}
}

void AUTPlayerController::ClientQueueCoolMoment_Implementation(FUniqueNetIdRepl NetId, float TimeToRewind)
{
	if (CVarUTEnableInstantReplay.GetValueOnGameThread() == 0)
	{
		return;
	}

	UE_LOG(UT, Log, TEXT("ClientQueueCoolMoment %f"), TimeToRewind);

	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetLocalPlayer());
	if (LocalPlayer != nullptr && LocalPlayer->GetKillcamPlaybackManager() != nullptr)
	{
		if (LocalPlayer->GetKillcamPlaybackManager()->IsEnabled() ||
			GetWorld()->GetTimerManager().IsTimerActive(KillcamStartHandle) ||
			GetWorld()->GetTimerManager().IsTimerActive(KillcamStopHandle))
		{
			// queue it up
			FQueuedCoolMoment QueuedCoolMoment;
			QueuedCoolMoment.NetId = NetId;
			QueuedCoolMoment.TimeToRewind = TimeToRewind;
			QueuedCoolMoments.Add(QueuedCoolMoment);
			return;
		}
		
		GetWorld()->GetTimerManager().SetTimer(
			KillcamStartHandle,
			FTimerDelegate::CreateUObject(this, &AUTPlayerController::OnCoolMomentReplayStart, NetId, TimeToRewind),
			CVarUTKillcamStartDelay.GetValueOnGameThread(),
			false);
	}
}

void AUTPlayerController::ClientPlayInstantReplay_Implementation(APawn* PawnToFocus, float TimeToRewind, float StartDelay)
{
	UE_LOG(UT, Log, TEXT("ClientPlayInstantReplay %f"), TimeToRewind);

	if (GetWorld()->DemoNetDriver && IsLocalController())
	{
		FNetworkGUID FocusPawnGuid = GetWorld()->DemoNetDriver->GetGUIDForActor(PawnToFocus);
		GetWorld()->GetTimerManager().SetTimer(
			KillcamStartHandle,
			FTimerDelegate::CreateUObject(this, &AUTPlayerController::OnKillcamStart, FocusPawnGuid, TimeToRewind),
			CVarUTKillcamStartDelay.GetValueOnGameThread(),
			false);
		GetWorld()->GetTimerManager().SetTimer(
			KillcamStopHandle,
			FTimerDelegate::CreateUObject(this, &AUTPlayerController::ClientStopKillcam),
			TimeToRewind + StartDelay,
			false);
	}
}

void AUTPlayerController::ClientPlayKillcam_Implementation(AController* KillingController, APawn* PawnToFocus, FVector_NetQuantize FocusLoc)
{
//	UE_LOG(UT, Log, TEXT("ClientPlayKillcam %d"), (GetWorld()->DemoNetDriver && IsLocalController()));
	if (Cast<AUTCharacter>(PawnToFocus) != nullptr)
	{
		DeathCamTime = GetWorld()->GetTimeSeconds();
		if (LineOfSightTo(PawnToFocus))
		{
			DeathCamFocus = PawnToFocus;
		}
		else
		{
			FActorSpawnParameters Params;
			Params.Instigator = PawnToFocus;
			Params.Owner = PawnToFocus;
			Params.bNoFail = true;
			AUTKillerTarget* KillerTarget = GetWorld()->SpawnActor<AUTKillerTarget>(AUTKillerTarget::StaticClass(), FocusLoc, PawnToFocus->GetActorRotation(), Params);
			if (KillerTarget != nullptr)
			{
				KillerTarget->InitFor(Cast<AUTCharacter>(PawnToFocus), this);
			}
			DeathCamFocus = KillerTarget;
		}
	}
	else
	{
		DeathCamFocus = nullptr;
	}
}

void AUTPlayerController::ClientStopKillcam_Implementation()
{
	UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetLocalPlayer());
	if (LocalPlayer != nullptr && LocalPlayer->GetKillcamPlaybackManager() != nullptr)
	{
		if (LocalPlayer->GetKillcamPlaybackManager()->IsEnabled())
		{
			FlushPressedKeys();
		}

		LocalPlayer->GetKillcamPlaybackManager()->KillcamStop();

		if (QueuedCoolMoments.Num() > 0)
		{
			OnCoolMomentReplayStart(QueuedCoolMoments[0].NetId, QueuedCoolMoments[0].TimeToRewind);
			QueuedCoolMoments.RemoveAt(0);
		}
	}
}

void AUTPlayerController::OnCoolMomentReplayStart(const FUniqueNetIdRepl NetId, float TimeToRewind)
{
	UE_LOG(UT, Log, TEXT("OnCoolMomentReplayStart %f"), TimeToRewind);

	// Show killcam
	if (IsLocalController())
	{
		// If the player unpresses any keys while inside the other world, they will still be pressed in this one
		FlushPressedKeys();

		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetLocalPlayer());
		if (LocalPlayer != nullptr && LocalPlayer->GetKillcamPlaybackManager() != nullptr)
		{
			if (LocalPlayer->GetKillcamPlaybackManager()->GetKillcamWorld() != GetWorld())
			{
				// The cool stuff peaked at TimeToRewind, go back a few seconds before that
				LocalPlayer->GetKillcamPlaybackManager()->CoolMomentCamStart(TimeToRewind + CVarUTCoolMomentRewindTime.GetValueOnGameThread(), NetId);
			}
			GetWorld()->GetTimerManager().SetTimer(
				KillcamStopHandle,
				FTimerDelegate::CreateUObject(this, &AUTPlayerController::ClientStopKillcam),
				CVarUTCoolMomentRewindTime.GetValueOnGameThread() + 0.5f,
				false);
		}
	}
}

void AUTPlayerController::OnKillcamStart(const FNetworkGUID InFocusActorGUID, float TimeToRewind)
{
	UE_LOG(UT, Log, TEXT("OnKillcamStart %f"), TimeToRewind);

	// Show killcam
	if (IsLocalController())
	{
		// If the player unpresses any keys while inside the other world, they will still be pressed in this one
		FlushPressedKeys();

		//if (IsInState(NAME_Spectating) || IsInState(NAME_Inactive))
		UUTLocalPlayer* LocalPlayer = Cast<UUTLocalPlayer>(GetLocalPlayer());
		if (LocalPlayer != nullptr && LocalPlayer->GetKillcamPlaybackManager() != nullptr)
		{
			if (LocalPlayer->GetKillcamPlaybackManager()->GetKillcamWorld() != GetWorld())
			{
				LocalPlayer->GetKillcamPlaybackManager()->KillcamStart(TimeToRewind, InFocusActorGUID);
			}
		}
	}
}

void AUTPlayerController::SendComsMessage(AUTPlayerState* Target, int32 Switch)
{
	if (GetNetMode() == NM_Client)
	{
		// Spam protection
		if (GetWorld()->GetRealTimeSeconds() - LastComMessageTime < (Switch == LastComMessageSwitch ? 5.0f : 1.5f))
		{
			return;
		}
		LastComMessageSwitch = Switch;
		LastComMessageTime = GetWorld()->GetRealTimeSeconds();
	}

	ServerSendComsMessage(Target, Switch);
}


bool AUTPlayerController::ServerSendComsMessage_Validate(AUTPlayerState* Target, int32 Switch) { return true; }
void AUTPlayerController::ServerSendComsMessage_Implementation(AUTPlayerState* Target, int32 Switch)
{
	// Spam protection
	if (GetWorld()->GetRealTimeSeconds() - LastComMessageTime < (Switch == LastComMessageSwitch ? 5.0f : 1.5f))
	{
		return;
	}

	LastComMessageSwitch = Switch;
	LastComMessageTime = GetWorld()->GetRealTimeSeconds();

	AUTGameMode* UTGameMode = GetWorld()->GetAuthGameMode<AUTGameMode>();
	if (UTGameMode != nullptr)
	{
		UTGameMode->SendComsMessage(this, Target, Switch);
	}
}

void AUTPlayerController::DumpMapVote()
{
	AUTGameState* GameState = GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->MapVoteList.Num() > 0)
	{
		for (int32 i=0; i < GameState->MapVoteList.Num(); i++)
		{
			UE_LOG(UT,Log,TEXT("MapVoteList[%i] Map = %s  Image = %s"), i, *GameState->MapVoteList[i]->MapPackageName, *GameState->MapVoteList[i]->MapScreenshot->GetFullName());
		}
	}
	else
	{
		UE_LOG(UT,Log,TEXT("Nothing in the mapvote list!"));
	}

}

bool AUTPlayerController::CanPerformRally() const
{
	AUTFlagRunGameState* GameState = GetWorld()->GetGameState<AUTFlagRunGameState>();
	return (GameState && UTPlayerState && UTPlayerState->bCanRally && UTPlayerState->Team && GameState->bAttackersCanRally && ((UTPlayerState->Team->TeamIndex == 0) == GameState->bRedToCap));
}

void AUTPlayerController::InitializeHeartbeatManager()
{
	if (!HeartbeatManager)
	{
		HeartbeatManager = NewObject<UUTHeartbeatManager>(this);
		HeartbeatManager->StartManager(this);
	}
}

EWeaponHand AUTPlayerController::GetPreferredWeaponHand()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return ReplicatedWeaponHand;
	}
	else
	{
		UUTProfileSettings* ProfileSettings = GetProfileSettings();
		return ProfileSettings ? ProfileSettings->WeaponHand : ReplicatedWeaponHand;
	}
}

void AUTPlayerController::ViewStartSpot()
{
	if (StartSpot != nullptr)
	{
		ChangeState(NAME_Spectating);
		ClientReset();
		// Set the player controller / camera in this new location
		ClientViewSpectatorPawn(FViewTargetTransitionParams());
		FRotator InitialControllerRot = StartSpot->GetActorRotation();
		InitialControllerRot.Roll = 0.f;
		SetInitialLocationAndRotation(StartSpot->GetActorLocation(), InitialControllerRot);
		ClientSetSpectatorLocation(StartSpot->GetActorLocation(), InitialControllerRot);
	}
}

void AUTPlayerController::PlayTutorialAnnouncement(int32 Index, UObject* OptionalObject) 
{
	FName NewAnnName = UUTTutorialAnnouncement::StaticClass()->GetDefaultObject<UUTTutorialAnnouncement>()->GetAnnouncementName(Index, OptionalObject, UTPlayerState, nullptr);
	int32 TutIndex = 0;
	if (!PlayedTutAnnouncements.Find(NewAnnName, TutIndex))
	{
		PlayedTutAnnouncements.Add(NewAnnName);
		ClientReceiveLocalizedMessage(UUTTutorialAnnouncement::StaticClass(), Index, UTPlayerState, nullptr, OptionalObject);

		if (FUTAnalytics::IsAvailable())
		{
			FUTAnalytics::FireEvent_UTTutorialPlayInstruction(Index, OptionalObject ? OptionalObject->GetName() : FString());
		}
	}
}

void AUTPlayerController::ClientSetLineUpCamera_Implementation(UWorld* World, LineUpTypes IntroType)
{
	AActor* Camera = AUTLineUpHelper::GetCameraActorForLineUp(World, IntroType);
	if (Camera)
	{
		FViewTargetTransitionParams TransitionParams;
		TransitionParams.BlendFunction = EViewTargetBlendFunction::VTBlend_Linear;

		if (World->GetGameState<AUTGameState>() && World->GetGameState<AUTGameState>()->GetMatchState() == MatchState::WaitingPostMatch)
		{
			FinalViewTarget = Camera;
		}

		SetViewTarget(Camera, TransitionParams);
	}
}


void AUTPlayerController::ClientSetActiveLineUp_Implementation(bool bNewIsActive, LineUpTypes LastType)
{
	if (GetWorld())
	{
		AUTGameState* UTGS = Cast<AUTGameState>(GetWorld()->GetGameState());
		if (UTGS && UTGS->LineUpHelper)
		{
			UTGS->LineUpHelper->bIsActive = bNewIsActive;
			UTGS->LineUpHelper->LastActiveType = LastType;

			if (bNewIsActive)
			{
				ClientSetLineUpCamera(GetWorld(), LastType);

				if (GetPawn())
				{
					GetPawn()->TurnOff();
				}

				SetIgnoreLookInput(true);

				ToggleScoreboard(false);
			}
			else
			{
				//Need to kill local pawn and then restart, so that we sync back up with the server
				if (GetPawn())
				{
					GetPawn()->Destroy();
				}
				ClientRestart(nullptr);
			}
		}
	}
}

bool AUTPlayerController::LineOfSightTo(const class AActor* Other, FVector ViewPoint, bool bAlternateChecks) const
{
	if (Other == NULL)
	{
		return false;
	}
	else
	{
		const AUTCharacter* CharacterTarget = Cast<AUTCharacter>(Other);
		const bool bOtherIsRagdoll = CharacterTarget && CharacterTarget->IsRagdoll();

		if (ViewPoint.IsZero())
		{
			AActor*	ViewTarg = GetPawn() ? GetPawn() : GetViewTarget();
			ViewPoint = ViewTarg->GetActorLocation();
			if (ViewTarg == GetPawn())
			{
				ViewPoint.Z += GetPawn()->BaseEyeHeight; //look from eyes
			}
		}

		static FName NAME_LineOfSight = FName(TEXT("LineOfSight"));
		FVector TargetLocation = Other->GetTargetLocation(GetPawn());
		FCollisionQueryParams CollisionParams(NAME_LineOfSight, true, GetPawn());
		CollisionParams.AddIgnoredActor(Other);
		FHitResult Hit;
		bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewPoint, TargetLocation, ECC_Visibility, CollisionParams);
		if (bHit && bOtherIsRagdoll)
		{
			// actor location will be near/in the ground for ragdolls, push up
			TargetLocation.Z += Other->GetSimpleCollisionHalfHeight();
			bHit = GetWorld()->LineTraceTestByChannel(ViewPoint, TargetLocation, ECC_Visibility, CollisionParams);
		}
		if (!bHit)
		{
			return true;
		}
		if (CharacterTarget)
		{
			bHit = GetWorld()->LineTraceTestByChannel(ViewPoint, CharacterTarget->GetActorLocation() + FVector(0.0f, 0.0f, CharacterTarget->BaseEyeHeight + (bOtherIsRagdoll ? CharacterTarget->GetSimpleCollisionHalfHeight() : 0.0f)), ECC_Visibility, CollisionParams);
		}
		return !bHit;
	}
}

void AUTPlayerController::ClientDrawLine_Implementation(FVector Start, FVector End) const
{
	DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, true);
}

void AUTPlayerController::RealNames()
{
	if (MyUTHUD && MyUTHUD->MyUTScoreboard)
	{
		MyUTHUD->MyUTScoreboard->bForceRealNames = true;
		AUTGameState* GS = GetWorld()->GetGameState<AUTGameState>();
		if (GS)
		{
			for (int32 i = 0; i < GS->PlayerArray.Num(); i++)
			{
				AUTPlayerState* PS = Cast<AUTPlayerState>(GS->PlayerArray[i]);
				if (PS)
				{
					TSharedRef<const FUniqueNetId> UserId = MakeShareable(new FUniqueNetIdString(*PS->StatsID));
					FText EpicAccountName = GS->GetEpicAccountNameForAccount(UserId);
					if (!EpicAccountName.IsEmpty())
					{
						PS->PlayerName = EpicAccountName.ToString();
					}
				}
			}
		}
	}
}

void AUTPlayerController::ClientReceiveLocalizedMessage_Implementation(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	Super::ClientReceiveLocalizedMessage_Implementation(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
	
	// Forward the local message over to the demo spectator
	if (GetWorld()->IsRecordingClientReplay())
	{
		UDemoNetDriver* DemoDriver = GetWorld()->DemoNetDriver;
		if (DemoDriver)
		{
			AUTDemoRecSpectator* DemoRecSpec = Cast<AUTDemoRecSpectator>(DemoDriver->SpectatorController);
			if (DemoRecSpec)
			{
				DemoRecSpec->MulticastReceiveLocalizedMessage(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
			}
		}
	}
}