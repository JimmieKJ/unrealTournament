	// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTBasePlayerController.h"
#include "UTPickupWeapon.h"
#include "UTPlayerController.generated.h"

// range user is allowed to configure FOV angle
#define FOV_CONFIG_MIN 80.0f
#define FOV_CONFIG_MAX 120.0f

class UUTAnnouncer;

struct FDeferredFireInput
{
	/** the fire mode */
	uint8 FireMode;
	/** if true, call StartFire(), false call StopFire() */
	bool bStartFire;

	FDeferredFireInput(uint8 InFireMode, bool bInStartFire)
		: FireMode(InFireMode), bStartFire(bInStartFire)
	{}
};


/** controls location and orientation of first person weapon */
UENUM()
enum EWeaponHand
{
	HAND_Right,
	HAND_Left,
	HAND_Center,
	HAND_Hidden,
};

UCLASS(config=Game)
class UNREALTOURNAMENT_API AUTPlayerController : public AUTBasePlayerController
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY()
	AUTCharacter* UTCharacter;

public:

	UFUNCTION(BlueprintCallable, Category = PlayerController)
	virtual AUTCharacter* GetUTCharacter();

	UPROPERTY(ReplicatedUsing = OnRep_HUDClass)
	TSubclassOf<class AHUD> HUDClass;

	UFUNCTION()
	virtual void OnRep_HUDClass();

	UPROPERTY()
	class AUTHUD* MyUTHUD;

	UPROPERTY(Config, BlueprintReadWrite, Category = Announcer)
	FStringClassReference AnnouncerPath;

	/** announcer for reward and staus messages - only set on client */
	UPROPERTY(BlueprintReadWrite, Category = Announcer)
	class UUTAnnouncer* Announcer;
	
	UPROPERTY(BlueprintReadWrite, Category = Sounds)
	USoundBase* ChatMsgSound;

	virtual void BeginPlay() override;
	virtual void Destroyed() override;
	virtual void InitInputSystem() override;
	virtual void InitPlayerState();
	virtual void OnRep_PlayerState();
	virtual void SetPawn(APawn* InPawn);
	virtual void SetupInputComponent() override;
	virtual void ProcessPlayerInput(const float DeltaTime, const bool bGamePaused) override;
	virtual void PawnPendingDestroy(APawn* InPawn) override;

	virtual void ClientRestart_Implementation(APawn* NewPawn) override;
	virtual void ClientSetLocation_Implementation(FVector NewLocation, FRotator NewRotation) override;

	virtual void BeginInactiveState() override;
	virtual void EndInactiveState() override;
	virtual void SpectateKiller();
	FTimerHandle SpectateKillerHandle;

	virtual void CheckAutoWeaponSwitch(class AUTWeapon* TestWeapon);

	UPROPERTY(GlobalConfig)
		bool bHearsTaunts;

	UPROPERTY()
		float LastSameTeamTime;

	/** check if sound is audible to this player and call ClientHearSound() if so to actually play it
	 * SoundPlayer may be NULL
	 */
	virtual void HearSound(USoundBase* InSoundCue, AActor* SoundPlayer, const FVector& SoundLocation, bool bStopWhenOwnerDestroyed, bool bAmplifyVolume);

	/** plays a heard sound locally
	 * SoundPlayer may be NULL for an unattached sound
	 * if SoundLocation is zero then the sound should be attached to SoundPlayer
	 */
	UFUNCTION(client, unreliable)
	void ClientHearSound(USoundBase* TheSound, AActor* SoundPlayer, FVector_NetQuantize SoundLocation, bool bStopWhenOwnerDestroyed, bool bOccluded, bool bAmplifyVolume);

	virtual void ClientSay_Implementation(AUTPlayerState* Speaker, const FString& Message, FName Destination) override
	{
		ClientPlaySound(ChatMsgSound);
		Super::ClientSay_Implementation(Speaker, Message, Destination);
	}

	UFUNCTION(exec)
	virtual void SwitchToBestWeapon();
	/** forces SwitchToBestWeapon() call, should only be used after granting startup inventory */
	UFUNCTION(Client, Reliable)
	virtual void ClientSwitchToBestWeapon();

	UFUNCTION(exec)
	virtual void NP();

	UFUNCTION(server, reliable, withvalidation)
	virtual void ServerNP();

	/** Notification from client that it detected a client side projectile hit (like a shock combo) */
	UFUNCTION(server, unreliable, withvalidation)
	virtual void ServerNotifyProjectileHit(AUTProjectile* HitProj, FVector HitLocation, AActor* DamageCauser, float TimeStamp);

	void AddWeaponPickup(class AUTPickupWeapon* NewPickup)
	{
		// clear out any dead entries for destroyed pickups
		for (TSet< TWeakObjectPtr<AUTPickupWeapon> >::TIterator It(RecentWeaponPickups); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}

		RecentWeaponPickups.Add(NewPickup);
	}

	virtual void UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents);

	UFUNCTION(exec)
	virtual void ToggleScoreboard(bool bShow);

	UFUNCTION(client, reliable)
	virtual void ClientToggleScoreboard(bool bShow);
	
	UFUNCTION(reliable, server, WithValidation)
	void ServerSelectSpawnPoint(APlayerStart* DesiredStart);

	/** Attempts to restart this player, generally called from the client upon respawn request. */
	UFUNCTION(reliable, server, WithValidation)
	void ServerRestartPlayerAltFire();

	/**	We overload ServerRestartPlayer so that we can set the bReadyToPlay flag if the game hasn't begun	 **/
	virtual void ServerRestartPlayer_Implementation();
	
	/**  Added a check to see if the player's RespawnTimer is > 0	 **/
	virtual bool CanRestartPlayer();

	virtual bool InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

	UPROPERTY(Replicated, BlueprintReadWrite, Category = Camera)
	bool bAllowPlayingBehindView;

	UFUNCTION(exec)
	virtual void BehindView(bool bWantBehindView);

	virtual bool IsBehindView();
	virtual void SetCameraMode( FName NewCamMode );
	virtual void ClientSetCameraMode_Implementation( FName NewCamMode ) override;
	virtual void ClientGameEnded_Implementation(AActor* EndGameFocus, bool bIsWinner) override;

	/** Handles bWantsBehindView. */
	virtual void ResetCameraMode() override;

	/** Timer function to bring up scoreboard after end of game. */
	virtual void ShowEndGameScoreboard();

	UFUNCTION(reliable, client)
	virtual void ClientReceiveXP(FXPBreakdown GainedXP);

	/**	Client replicated function that get's called when it's half-time. */
	UFUNCTION(client, reliable)
	void ClientHalftime();

	/** Switch to best current camera while spectating. */
	virtual void ChooseBestCamera();

	/** If true, show networking stats widget on HUD. */
	UPROPERTY()
		bool bShowNetInfo;

	/** Toggle showing net stats on HUD. */
	UFUNCTION(exec)
		virtual void NetStats();

	virtual void SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams()) override;
	virtual void ServerViewSelf_Implementation(FViewTargetTransitionParams TransitionParams) override;
	virtual void ViewSelf(FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());

	/** Update rotation to be good view of current viewtarget.  UnBlockedPct is how much of the camera offset trace needs to be unblocked. */
	UFUNCTION()
		virtual void FindGoodView(const FVector& TargetLoc, bool bIsUpdate);

	UPROPERTY()
		float LastGoalYaw;

	UFUNCTION(Client, Reliable)
	void ClientViewSpectatorPawn(FViewTargetTransitionParams TransitionParams);

	UFUNCTION(exec)
	virtual void ViewPlayerNum(int32 Index, uint8 TeamNum = 255);

	UFUNCTION(exec)
	virtual void ViewNextPlayer();

	/** View Player holding flag specified by TeamIndex. */
	UFUNCTION(unreliable, server, WithValidation)
	void ServerViewFlagHolder(uint8 TeamIndex);

	/** View last projectile fired by currently viewed player. */
	UFUNCTION(unreliable, server, WithValidation)
	void ServerViewProjectile();

	/** View character associated with playerstate. */
	UFUNCTION(unreliable, server, WithValidation)
	void ServerViewPlayerState(APlayerState* PS);

	virtual void ViewPlayerState(APlayerState* PS);

	UFUNCTION(exec)
	virtual void ViewClosestVisiblePlayer();

	UFUNCTION(exec)
	virtual void ViewProjectile();

	virtual void ServerViewProjectileShim();

	UFUNCTION(exec)
	virtual void ViewFlag(uint8 Index);

	UFUNCTION(exec)
	virtual void ViewCamera(int32 Index);

	UFUNCTION(exec)
		virtual void StartCameraControl();

	UFUNCTION(exec)
		virtual void EndCameraControl();

	/** Returns updated rotation for third person camera view. */
	UFUNCTION()
		virtual FRotator GetSpectatingRotation(const FVector& ViewLoc, float DeltaTime);

	UFUNCTION(exec)
	virtual void ToggleTacCom();

	/** Enables auto cam (auto cam turned off whenever use a spectator camera bind. */
	UFUNCTION(exec)
		virtual void EnableAutoCam();

	/** Enables Auto best camera for spectators. */
	UPROPERTY(BluePrintReadWrite)
		bool bAutoCam;

	/** Enables TacCom for spectators. */
	UPROPERTY(BluePrintReadWrite)
	bool bTacComView;

	virtual void UpdateTacComOverlays();

	/** View Flag of team specified by Index. */
	UFUNCTION(unreliable, server, WithValidation)
	void ServerViewFlag(uint8 Index);

	virtual FVector GetFocalLocation() const override;

	virtual void Possess(APawn*) override;
	virtual void PawnLeavingGame() override;

	/**	We override player tick to keep updating the player's rotation when the game is over. */
	virtual void PlayerTick(float DeltaTime) override;

	virtual void Tick(float DeltaTime) override;

	virtual void NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyTakeHit(bool bFriendlyFire, uint8 Damage, FVector_NetQuantize RelHitLocation);

	/** notification that we successfully hit HitPawn
	 * note that HitPawn may be NULL if it is not currently relevant to the client
	 */
	UFUNCTION(Client, Unreliable)
	void ClientNotifyCausedHit(APawn* HitPawn, uint8 Damage);

	/** blueprint hook */
	UFUNCTION(BlueprintCallable, Category = Message)
	void K2_ReceiveLocalizedMessage(TSubclassOf<ULocalMessage> Message, int32 Switch = 0, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL);

	UPROPERTY(GlobalConfig, BlueprintReadOnly, Category = Weapon)
	bool bAutoWeaponSwitch;

	/** Global scaling for weapon bob. */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = WeaponBob)
	float WeaponBobGlobalScaling;

	/** Global scaling for eye offset. */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = WeaponBob)
	float EyeOffsetGlobalScaling;

	UFUNCTION(exec)
	virtual void SetEyeOffsetScaling(float NewScaling);

	UFUNCTION(exec)
	virtual void SetWeaponBobScaling(float NewScaling);

	/** If true, will slide if running and press crouch. */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Movement)
		bool bAllowSlideFromRun;

	/** If true, single quick tap will result in wall dodge on release.  Otherwise need double tap to wall dodge. */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Movement)
	bool bSingleTapWallDodge;

	/** If true (and bSingleTapWallDodge is true), single tap wall dodge only enabled after intentional jump. */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Movement)
	bool bSingleTapAfterJump;

	/** Toggles bSingleTapWallDodge */
	UFUNCTION(exec)
	virtual void ToggleSingleTap();

	UPROPERTY(EditAnywhere, GlobalConfig, Category = Camera)
	int32 StylizedPPIndex;

	UFUNCTION(exec)
	virtual void SetStylizedPP(int32 NewPP);

	UFUNCTION(exec)
	virtual void DemoRestart();

	UFUNCTION(exec)
	virtual void DemoSeek(float DeltaSeconds);

	UFUNCTION(exec)
	virtual void DemoGoTo(float Seconds);

	UFUNCTION(exec)
	virtual void DemoGoToLive();

	UFUNCTION(exec)
	virtual void DemoPause();

	UFUNCTION(exec)
	virtual void DemoTimeDilation(float DeltaAmount);

	UFUNCTION(exec)
	virtual void DemoSetTimeDilation(float Amount);

	virtual void OnDemoSeeking();

	/** whether player wants behindview when spectating */
	UPROPERTY(BlueprintReadWrite, GlobalConfig)
	bool bSpectateBehindView;

	/** Whether should remaing in freecam on camera resets. */
	UPROPERTY(BlueprintReadOnly)
		bool bCurrentlyBehindView;

	UPROPERTY(BlueprintReadOnly)
		bool bRequestingSlideOut;

	/** True when spectator has used a spectating camera bind. */
	UPROPERTY()
		bool bHasUsedSpectatingBind;

	UPROPERTY()
		bool bShowCameraBinds;

	UFUNCTION(exec)
		virtual void ToggleSlideOut();

	UFUNCTION(exec)
		virtual void ToggleShowBinds();

	UFUNCTION(exec)
	virtual void TogglePlayerInfo();

	virtual void ViewAPlayer(int32 dir)
	{
		BehindView(bSpectateBehindView);

		Super::ViewAPlayer(dir);
	}

	/** Toggle behindview for spectators. */
	UFUNCTION(exec)
		virtual void ToggleBehindView();

	/** user configurable FOV setting */
	UPROPERTY(BlueprintReadOnly, GlobalConfig, Category = Camera)
	float ConfigDefaultFOV;

	virtual void SpawnPlayerCameraManager() override;
	virtual void FOV(float NewFOV) override;

	/** desired "team" color for players in FFA games */
	UPROPERTY(BlueprintReadOnly, GlobalConfig, Category = Display)
	FLinearColor FFAPlayerColor;

	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

	float LastEmoteTime;
	
	float EmoteCooldownTime;

	UFUNCTION(Exec)
	virtual void Emote(int32 EmoteIndex);

	UFUNCTION(reliable, server, WithValidation)
	virtual void ServerEmote(int32 EmoteIndex);

	UFUNCTION()
	virtual void FasterEmote();

	UFUNCTION()
	virtual void SlowerEmote();

	UFUNCTION(Exec)
	virtual void SetEmoteSpeed(float NewEmoteSpeed);

	UFUNCTION()
	virtual void PlayTaunt();

	UFUNCTION()
	virtual void PlayTaunt2();

	UFUNCTION(Exec)
	virtual void SetMouseSensitivityUT(float NewSensitivity);

	UPROPERTY()
	class AUTPlayerState* LastSpectatedPlayerState;

	UPROPERTY()
	int32 LastSpectatedPlayerId;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerViewPawn(APawn* PawnToView);

	virtual void ViewPawn(APawn* PawnToView);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerViewPlaceholderAtLocation(FVector Location);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = UISound)
		USoundBase* SelectSound;
	
	UFUNCTION()
	virtual void PlayMenuSelectSound();

	UFUNCTION(exec)
	void ClearTokens();

	//-----------------------------------------------
	// Perceived latency reduction
	/** Used to correct prediction error. */
	UPROPERTY(EditAnywhere, Replicated, GlobalConfig, Category=Network)
	float PredictionFudgeFactor;

	/** Negotiated max amount of ping to predict ahead for. */
	UPROPERTY(BlueprintReadOnly, Category=Network, Replicated)
	float MaxPredictionPing;

	/** user configurable desired prediction ping (will be negotiated with server. */
	UPROPERTY(BlueprintReadOnly, GlobalConfig, Category=Network)
	float DesiredPredictionPing;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Debug)
		bool bIsDebuggingProjectiles;

	/** Propose a desired ping to server */
	UFUNCTION(reliable, server, WithValidation)
	virtual void ServerNegotiatePredictionPing(float NewPredictionPing);

	/** Console command to change prediction ping */
	UFUNCTION(Exec)
	virtual void Predict(float NewDesiredPredictionPing);

	/** Return amount of time to tick or simulate to make up for network lag */
	virtual float GetPredictionTime();

	/** How long fake projectile should sleep before starting to simulate (because client ping is greater than MaxPredictionPing). */
	virtual float GetProjectileSleepTime();

	/** List of fake projectiles currently out there for this client */
	UPROPERTY()
	TArray<class AUTProjectile*> FakeProjectiles;

	//-----------------------------------------------
	// Ping calculation

	/** Last time this client's ping was updated. */
	UPROPERTY()
	float LastPingCalcTime;

	/** Client sends ping request to server - used when servermoves aren't happening. */
	UFUNCTION(unreliable, server, WithValidation)
	virtual void ServerBouncePing(float TimeStamp);

	/** Server bounces ping request back to client - used when servermoves aren't happening. */
	UFUNCTION(unreliable, client)
	virtual void ClientReturnPing(float TimeStamp);

	/** Client informs server of new ping update. */
	UFUNCTION(unreliable, server, WithValidation)
	virtual void ServerUpdatePing(float ExactPing);

	//-----------------------------------------------
	/** guess of this player's target on last shot, used by AI */
	UPROPERTY(BlueprintReadWrite, Category = AI)
	APawn* LastShotTargetGuess;

	virtual float GetWeaponAutoSwitchPriority(FString WeaponClassname, float DefaultPriority);

	virtual void ClientRequireContentItemListComplete_Implementation() override;

	/** sent from server when it accepts the URL parameter "?castingguide=1", which enables a special multi-camera view that shows many potential spectating views at once */
	UPROPERTY(ReplicatedUsing = OnRep_CastingGuide)
	bool bCastingGuide;
	/** casting guide view number, 0 == primary PC, 1+ == child PCs */
	UPROPERTY(ReplicatedUsing = OnRep_CastingViewIndex)
	int32 CastingGuideViewIndex;

	/** default view commands for each CastingGuideViewIndex */
	UPROPERTY(Config)
	TArray<FString> CastingGuideStartupCommands;

	UFUNCTION()
	void OnRep_CastingGuide();
	UFUNCTION()
	void OnRep_CastingViewIndex();

	UFUNCTION(exec)
	virtual void StartCastingGuide();
	UFUNCTION(Reliable, Server, WithValidation)
	void ServerStartCastingGuide();

	UFUNCTION(Exec)
	virtual void RconMap(FString NewMap);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRconMap(const FString& NewMap);

	UFUNCTION(Exec)
	virtual void RconNextMap(FString NextMap);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRconNextMap(const FString& NextMap);

	UFUNCTION(Exec)
	virtual void UTBugIt(const FString& ScreenShotDescription);
	virtual void UTBugItStringCreator(FVector ViewLocation, FRotator ViewRotation, FString& GoString, FString& LocString);
	virtual void UTLogOutBugItGoToLogFile(const FString& InScreenShotDesc, const FString& InGoString, const FString& InLocString);
protected:

	// If set, this will be the final viewtarget this pawn can see.
	UPROPERTY()
	AActor* FinalViewTarget;

	/** list of weapon pickups that my Pawn has recently picked up, so we can hide the weapon mesh per player */
	TSet< TWeakObjectPtr<AUTPickupWeapon> > RecentWeaponPickups;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
	float BaseLookUpRate;

	/** Current movement axis deflecton forward/back (back is negative) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement)
	float MovementForwardAxis;

	/** Current movement axis deflecton right/left (left is negative) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Movement)
	float MovementStrafeAxis;

	UPROPERTY(config)
	float KillerSpectateDelay;

public:
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Dodging)
	float MaxDodgeClickTime;

	/** Max held time for single tap wall dodge */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Dodging)
	float MaxDodgeTapTime;

	/** Use classic weapon groups. */
	UPROPERTY(globalconfig)
		bool bUseClassicGroups;

	/** Switch between teams 0 and 1 */
	UFUNCTION(exec)
	virtual void SwitchTeam();

protected:
	UPROPERTY(globalconfig, BlueprintReadOnly, Category = Weapon)
	TEnumAsByte<EWeaponHand> WeaponHand;
public:
	inline EWeaponHand GetWeaponHand() const
	{
		//Spectators always see right handed weapons
		return IsInState(NAME_Spectating) ? HAND_Right : GetPreferredWeaponHand();
	}

	inline EWeaponHand GetPreferredWeaponHand() const
	{
		return WeaponHand;
	}

	UFUNCTION(BlueprintCallable, Category = Weapon)
	void SetWeaponHand(EWeaponHand NewHand);

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerSetWeaponHand(EWeaponHand NewHand);

protected:
	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	float LastTapLeftTime;

	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	float LastTapRightTime;

	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	float LastTapForwardTime;

	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	float LastTapBackTime;

	/** if true, single tap dodge requested */
	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	bool bRequestedDodge;

	/** If true, holding dodge modifier key down, single tap of movement key causes dodge. */
	UPROPERTY(BluePrintReadOnly, Category = Dodging)
	bool bIsHoldingDodge;

	/** True if player is holding modifier to slide/roll */
	UPROPERTY(Category = "FloorSlide", BlueprintReadOnly)
	bool bIsHoldingFloorSlide;

	/** requests a change team; default is to switch to any other team than current */
	UFUNCTION(exec)
	virtual void ChangeTeam(uint8 NewTeamIndex = 255);

	UFUNCTION(exec)
	virtual void Suicide();
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSuicide();

	/** weapon selection */
	void PrevWeapon();
	void NextWeapon();

	UFUNCTION(exec)
	void ToggleTranslocator();

	void ThrowWeapon();
	
	UFUNCTION(Reliable, Server, WithValidation)
	virtual void ServerThrowWeapon();

	int32 PreviousWeaponGroup;

	virtual void SwitchWeaponInSequence(bool bPrev);

	/** Switches weapons using classic groups. */
	UFUNCTION(Exec)
	virtual void SwitchWeapon(int32 Group);

	/** Switches weapons using modern groups. */
	UFUNCTION(Exec)
	virtual void SwitchWeaponGroup(int32 Group);

	/** weapon fire input handling -- NOTE: Just forward to the pawn */
	virtual void OnFire();
	virtual void OnStopFire();
	virtual void OnAltFire();
	virtual void OnStopAltFire();

	/** Handles moving forward */
	virtual void MoveForward(float Val);

	/** Handles moving backward */
	virtual void MoveBackward(float Val);

	/** Handles strafing movement left */
	virtual void MoveLeft(float Val);

	/** Handles strafing movement right */
	virtual void MoveRight(float Val);

	/** Up and down when flying or swimming */
	virtual void MoveUp(float Val);

	/**
	* Called via input to turn at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	virtual void TurnAtRate(float Rate);

	/**
	* Called via input to turn look up/down at a given rate.
	* @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	*/
	virtual void LookUpAtRate(float Rate);

	/** called to set the jump flag from input */
	virtual void Jump();

	/** called when jump is released. */
	virtual void JumpRelease();

	virtual void Crouch();
	virtual void UnCrouch();
	virtual void ToggleCrouch();

	/** Handler for a touch input beginning. */
	void TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location);

	/** If double tap, tell pawn to dodge */
	void CheckDodge(float LastTapTime, float MaxClickTime, bool bForward, bool bBack, bool bLeft, bool bRight);

	/** Dodge tap input handling */
	void OnTapLeft();
	void OnTapRight();
	void OnTapForward();
	void OnTapBack();
	void OnTapLeftRelease();
	void OnTapRightRelease();
	void OnTapForwardRelease();
	void OnTapBackRelease();

	void OnSingleTapDodge();
	virtual void PerformSingleTapDodge();
	void HoldDodge();
	void ReleaseDodge();

	virtual void OnShowScores();
	virtual void OnHideScores();
	virtual void TestResult(uint16 ButtonID);

	virtual void ReceivedPlayer();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveStatsID(const FString& NewStatsID);

	/** stores fire inputs until after movement has been executed (default would be fire -> movement -> render, this causes movement -> fire -> render)
	 * makes weapons feel a little more responsive while strafing
	 */
	TArray< FDeferredFireInput, TInlineAllocator<2> > DeferredFireInputs;
public:
	void ApplyDeferredFireInputs();

	bool HasDeferredFireInputs();

	UFUNCTION(Exec)
	virtual void HUDSettings();

	// Will query the input system and return the FText with the name of the key to perform a command.  NOTE: it returns the version binding for that command 
	// that is found.
	UFUNCTION(BlueprintCallable, Category = Input)
	void ResolveKeybindToFKey(FString Command, TArray<FKey>& Keys, bool bIncludeGamepad=false, bool bIncludeAxis=true);

	// Will query the input system and return the FText with the name of the key to perform a command.  NOTE: it returns the version binding for that command 
	// that is found.
	UFUNCTION(BlueprintCallable, Category = Input)
	void ResolveKeybind(FString Command, TArray<FString>& Keys, bool bIncludeGamepad=false, bool bIncludeAxis=true);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveCountryFlag(FName NewCountryFlag);

	virtual void DebugTest(FString TestCommand) override;

protected:
	int32 ParseWeaponBind(FString ActionName);
	FString FixedupKeyname(FString KeyName);

	void TurnOffPawns();

public:
	TMap<int32,FString> WeaponGroupKeys;
	virtual void UpdateWeaponGroupKeys();

	UFUNCTION(server, reliable, withvalidation)
	void ServerRegisterBanVote(AUTPlayerState* BadGuy);
	
	virtual void UpdateRotation(float DeltaTime) override;

	UFUNCTION(client, reliable)
	void ClientOpenLoadout(bool bBuyMenu);

	UFUNCTION(Exec)
	void ShowBuyMenu();

	/** send localized message to this PC's client and to spectators of this PC's pawn. */
	virtual void SendPersonalMessage(TSubclassOf<ULocalMessage> Message, int32 Switch = 0, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL);

	/** Playerstate whose details are currently being displayed on scoreboard. */
	UPROPERTY()
	AUTPlayerState* CurrentlyViewedScorePS;

	UPROPERTY()
	int32 TeamStatsUpdateTeam;

	UPROPERTY()
	int32 TeamStatsUpdateIndex;

	UPROPERTY()
	float LastTeamStatsUpdateStartTime;

	UPROPERTY()
	int32 StatsUpdateIndex;

	UPROPERTY()
	float LastScoreStatsUpdateStartTime;

	UPROPERTY()
	uint8 CurrentlyViewedStatsTab;

	UFUNCTION()
	virtual void SetViewedScorePS(AUTPlayerState* ViewedPS, uint8 NewStatsPage);

	UFUNCTION(server, unreliable, withvalidation)
	virtual void ServerSetViewedScorePS(AUTPlayerState* ViewedPS, uint8 NewStatsPage);

	UFUNCTION(client, unreliable)
		virtual void ClientUpdateScoreStats(AUTPlayerState* ViewedPS, uint8 StatsPage, uint8 StatsIndex, float NewValue);

	UFUNCTION(client, unreliable)
	virtual void ClientUpdateTeamStats(uint8 TeamNum, uint8 TeamStatsIndex, float NewValue);

	virtual void AdvanceStatsPage(int32 Increment);

	int32 DilationIndex;

	UFUNCTION(client, reliable)
	virtual void ClientShowMapVote();

	UFUNCTION(client, reliable)
	virtual void ClientHideMapVote();

	UFUNCTION(exec)
	virtual void TestVote()
	{
		ClientShowMapVote();
	}

	/** Make sure no firing and scoreboard hidden before bringing up menu. */
	virtual void ShowMenu() override;
	
	AUTCharacter* PreGhostChar;

	/** When looking at a GhostCharacter, this will possess the GhostCharacter and begin recording */
	UFUNCTION(exec)
	virtual void GhostStart();

	/** Stops the recording GhostCharacter */
	UFUNCTION(exec)
	virtual void GhostStop();

	/** Plays the recording of the GhostCharacter you are looking at */
	UFUNCTION(exec)
	virtual void GhostPlay();

	class AUTCharacter* GhostTrace();


	UFUNCTION(exec)
	virtual void OpenMatchSummary();
};



