	// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTATypes.h"
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

UCLASS(config=Game)
class UNREALTOURNAMENT_API AUTPlayerController : public APlayerController, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY()
	AUTCharacter* UTCharacter;

public:

	UFUNCTION(BlueprintCallable, Category = PlayerController)
	virtual AUTCharacter* GetUTCharacter();

	UPROPERTY()
	AUTPlayerState* UTPlayerState;

	UPROPERTY()
	class AUTHUD* MyUTHUD;

	// the announcer types are split as we use different voices for the two types;
	// this allows them independent queues with limited talking over each other, which is better than the long queues that can sometimes happen
	// more configurability for mods and such doesn't hurt either

	UPROPERTY(Config, BlueprintReadWrite, Category = Announcer)
	FStringClassReference RewardAnnouncerPath;

	/** announcer for reward announcements (multikill, etc) - only set on client */
	UPROPERTY(BlueprintReadWrite, Category = Announcer)
	class UUTAnnouncer* RewardAnnouncer;

	UPROPERTY(Config, BlueprintReadWrite, Category = Announcer)
	FStringClassReference StatusAnnouncerPath;

	/** announcer for status announcements (red flag taken, etc) - only set on client */
	UPROPERTY(BlueprintReadWrite, Category = Announcer)
	class UUTAnnouncer* StatusAnnouncer;

	virtual void InitInputSystem() override;
	virtual void InitPlayerState();
	virtual void OnRep_PlayerState();
	virtual void SetPawn(APawn* InPawn);
	virtual void SetupInputComponent() override;
	virtual void ProcessPlayerInput(const float DeltaTime, const bool bGamePaused) override;
	virtual void PostInitializeComponents() override;

	virtual void ClientRestart_Implementation(APawn* NewPawn) override;

	virtual void CheckAutoWeaponSwitch(AUTWeapon* TestWeapon);

	/** check if sound is audible to this player and call ClientHearSound() if so to actually play it
	 * SoundPlayer may be NULL
	 */
	virtual void HearSound(USoundBase* InSoundCue, AActor* SoundPlayer, const FVector& SoundLocation, bool bStopWhenOwnerDestroyed, bool bAmplifyVolume);

	/** plays a heard sound locally
	 * SoundPlayer may be NULL for an unattached sound
	 * if SoundLocation is zero then the sound should be attached to SoundPlayer
	 */
	UFUNCTION(client, unreliable)
	void ClientHearSound(USoundBase* TheSound, AActor* SoundPlayer, FVector SoundLocation, bool bStopWhenOwnerDestroyed, bool bOccluded, bool bAmplifyVolume);

	UFUNCTION(exec)
	virtual void SwitchToBestWeapon();

	inline void AddWeaponPickup(class AUTPickupWeapon* NewPickup)
	{
		// insert new pickups at the beginning so the order should be newest->oldest
		// this makes iteration and removal faster when deciding whether the pickup is still hidden in the per-frame code
		RecentWeaponPickups.Insert(NewPickup, 0);
	}

	virtual void UpdateHiddenComponents(const FVector& ViewLocation, TSet<FPrimitiveComponentId>& HiddenComponents);

	virtual void SetName(const FString& S);

	UFUNCTION(exec)
	virtual void ToggleScoreboard(bool bShow);

	UFUNCTION(client, reliable)
	virtual void ClientToggleScoreboard(bool bShow);

	UFUNCTION(client, reliable)
	virtual void ClientSetHUDAndScoreboard(TSubclassOf<class AHUD> NewHUDClass, TSubclassOf<class UUTScoreboard> NewScoreboardClass);

	/**	We overload ServerRestartPlayer so that we can set the bReadyToPlay flag if the game hasn't begun	 **/
	virtual void ServerRestartPlayer_Implementation();

	/**  Added a check to see if the player's RespawnTimer is > 0	 **/
	virtual bool CanRestartPlayer();

	virtual bool InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad) override;

	UFUNCTION(exec)
	virtual void BehindView(bool bWantBehindView);

	UPROPERTY()
	int32 BehindViewStacks;

	virtual bool IsBehindView();
	virtual void SetCameraMode( FName NewCamMode );
	virtual void ClientSetCameraMode_Implementation( FName NewCamMode ) override;
	virtual void ClientGameEnded_Implementation(AActor* EndGameFocus, bool bIsWinner) override;

	/**	Client replicated function that get's called when it's half-time. */
	UFUNCTION(client, reliable)
	virtual void ClientHalftime();

	virtual void SetViewTarget(class AActor* NewViewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams());

	virtual FVector GetFocalLocation() const override;

	// A quick function so I don't have to keep adding one when I want to test something.  @REMOVEME: Before the final version
	UFUNCTION(exec)
	virtual void DebugTest(FString TestCommand);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerDebugTest();

	virtual void PawnLeavingGame() override;

	/**	We override player tick to keep updating the player's rotation when the game is over. */
	virtual void PlayerTick(float DeltaTime);

	void NotifyTakeHit(AController* InstigatedBy, int32 Damage, FVector Momentum, const FDamageEvent& DamageEvent);

	UFUNCTION(Client, Unreliable)
	void ClientNotifyTakeHit(APlayerState* InstigatedBy, int32 Damage, FVector Momentum, FVector RelHitLocation, TSubclassOf<UDamageType> DamageType);

	/** notification that we successfully hit HitPawn
	 * note that HitPawn may be NULL if it is not currently relevant to the client
	 */
	UFUNCTION(Client, Unreliable)
	void ClientNotifyCausedHit(APawn* HitPawn, int32 Damage);

	/**	Will popup the in-game menu	 **/
	UFUNCTION(exec)
	virtual void ShowMenu();

	UFUNCTION(exec)
	virtual void HideMenu();

	virtual void ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback = FDialogResultDelegate());

	/** blueprint hook */
	UFUNCTION(BlueprintCallable, Category = Message)
	void K2_ReceiveLocalizedMessage(TSubclassOf<ULocalMessage> Message, int32 Switch = 0, class APlayerState* RelatedPlayerState_1 = NULL, class APlayerState* RelatedPlayerState_2 = NULL, class UObject* OptionalObject = NULL);

	virtual uint8 GetTeamNum() const;

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

	/** If true, single quick tap will result in wall dodge on release.  Otherwise need double tap to wall dodge. */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Movement)
	bool bSingleTapWallDodge;

	/** If true, holding slide/roll (shift) will keep current acceleration if no movement keys are pressed. */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Movement)
	bool bHoldAccelWithSlideRoll;

	/** Toggles bSingleTapWallDodge */
	UFUNCTION(exec)
	virtual void ToggleSingleTap();

	/** Toggles holding acceleration with slide/roll (shift) */
	UFUNCTION(exec)
	virtual void ToggleHoldAccel();

	/** If true, auto-slide, otherwise need to hold shift down to slide along walls. */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Movement)
	bool bAutoSlide;

	/** Toggles whether need to hold shift down or not to slide along walls. */
	UFUNCTION(exec)
	virtual void ToggleAutoSlide();

	/** Handles propagating autoslide changes to UTCharacterMovement and to server */
	virtual	void SetAutoSlide(bool bNewAutoSlide);

	/** Replicate autoslide setting to server */
	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetAutoSlide(bool bNewAutoSlide);

	/** user configurable FOV setting */
	UPROPERTY(BlueprintReadOnly, GlobalConfig, Category = Camera)
	float ConfigDefaultFOV;

	virtual void SpawnPlayerCameraManager() override;
	virtual void FOV(float NewFOV) override;

	/** desired "team" color for players in FFA games */
	UPROPERTY(BlueprintReadOnly, GlobalConfig, Category = Display)
	FLinearColor FFAPlayerColor;

	virtual void DisplayDebug(class UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos) override;

	UFUNCTION()
	virtual void Talk();

	UFUNCTION()
	virtual void TeamTalk();

	UFUNCTION(exec)
	virtual void Say(FString Message);

	UFUNCTION(exec)
	virtual void TeamSay(FString Message);

	UFUNCTION(reliable, server, WithValidation)
	virtual void ServerSay(const FString& Message, bool bTeamMessage);

	UFUNCTION(reliable, client)
	virtual void ClientSay(class AUTPlayerState* Speaker, const FString& Message, bool bTeamMessage);

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

	virtual void ClientSetViewTarget_Implementation(AActor* A, FViewTargetTransitionParams TransitionParams) override;

	UPROPERTY()
	class AUTCharacter* LastSpectatedCharacter;

	UPROPERTY()
	class APlayerState* LastSpectatedPlayerState;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerViewPawn(APawn* PawnToView);

protected:

	// If set, this will be the final viewtarget this pawn can see.
	AActor* FinalViewTarget;

	/** list of weapon pickups that my Pawn has recently picked up, so we can hide the weapon mesh per player */
	UPROPERTY()
	TArray<class AUTPickupWeapon*> RecentWeaponPickups;

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
public:
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Dodging)
	float MaxDodgeClickTime;

	/** Max held time for single tap wall dodge */
	UPROPERTY(EditAnywhere, GlobalConfig, Category = Dodging)
	float MaxDodgeTapTime;
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
	UPROPERTY(Category = "DodgeRoll", BlueprintReadOnly)
	bool bIsHoldingSlideRoll;

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
	void ToggleTranslocator();
	int32 PreviousWeaponGroup;

	virtual void SwitchWeaponInSequence(bool bPrev);
	UFUNCTION(Exec)
	virtual void SwitchWeapon(int32 Group);

	/** weapon fire input handling -- NOTE: Just forward to the pawn */
	virtual void OnFire();
	virtual void OnStopFire();
	virtual void OnAltFire();
	virtual void OnStopAltFire();

	/** Handles moving forward/backward */
	virtual void MoveForward(float Val);

	/** Handles strafing movement, left and right */
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
	void HoldRollSlide();
	void ReleaseRollSlide();

	virtual void OnShowScores();
	virtual void OnHideScores();
	virtual void TestResult(uint16 ButtonID);

	virtual void ReceivedPlayer();

	/** stores fire inputs until after movement has been executed (default would be fire -> movement -> render, this causes movement -> fire -> render)
	 * makes weapons feel a little more responsive while strafing
	 */
	TArray< FDeferredFireInput, TInlineAllocator<2> > DeferredFireInputs;
public:
	void ApplyDeferredFireInputs();
};



