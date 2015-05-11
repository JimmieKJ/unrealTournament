// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTeamInterface.h"
#include "Stat.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTDamageType.h"
#include "UTHat.h"
#include "UTEyewear.h"
#include "UTTaunt.h"
#include "UTPlayerState.generated.h"

USTRUCT(BlueprintType)
struct FWeaponSpree
{
	GENERATED_USTRUCT_BODY()

	FWeaponSpree() : SpreeSoundName(NAME_None), Kills(0) {};

	FWeaponSpree(FName InSpreeSoundName) : SpreeSoundName(InSpreeSoundName), Kills(0) {};

	UPROPERTY()
	FName SpreeSoundName;

	UPROPERTY()
	int32 Kills;
};

struct FTempBanInfo
{
	// The person who voted for this ban
	TSharedPtr<class FUniqueNetId> Voter;

	// When was the vote cast.  Votes will time out over time (5mins)
	float BanTime;

	FTempBanInfo(const TSharedPtr<class FUniqueNetId> inVoter, float inBanTime)
		: Voter(inVoter)
		, BanTime(inBanTime)
	{
	}

};



UCLASS()
class UNREALTOURNAMENT_API AUTPlayerState : public APlayerState, public IUTTeamInterface
{
	GENERATED_UCLASS_BODY()

private:

	/** Instance of a stat manager that tracks gameplay/reward stats for this Player Controller */
	UPROPERTY()
	class UStatManager *StatManager;

protected:
	/** selected character (if NULL left at default) */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = NotifyTeamChanged, Category = PlayerState)
	TSubclassOf<class AUTCharacterContent> SelectedCharacter;

	/** Used to optimize GetUTCharacter(). */
	UPROPERTY()
	AUTCharacter* CachedCharacter;

public:
	UFUNCTION(BlueprintCallable, Category = Character)
	virtual void SetCharacter(const FString& CharacterPath);
	
	UFUNCTION(BlueprintCallable, Reliable, Server, WithValidation, Category = Character)
	void ServerSetCharacter(const FString& CharacterPath);
	inline TSubclassOf<class AUTCharacterContent> GetSelectedCharacter() const
	{
		return SelectedCharacter;
	}

	/** Don't do engine style ping updating. */
	virtual void UpdatePing(float InPing) override;

	/** Called on client using the roundtrip time for servermove/ack. */
	virtual void CalculatePing(float NewPing);

	/** ID that can be used to consistently identify this player for spectating commands
	 * IDs are reused when players leave and new ones join, but a given player's ID remains stable and unique
	 * as long as that player is in the game
	 * NOTE: 1-based, zero is unassigned (bOnlySpectator spectators don't get a value)
	 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = PlayerState)
	uint8 SpectatingID;
	/** version of SpectatingID that is unique only amongst this player's team instead of all players in the game */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = PlayerState)
	uint8 SpectatingIDTeam;

	/** player's team if we're playing a team game */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = NotifyTeamChanged, Category = PlayerState)
	class AUTTeamInfo* Team;

	/** Whether this player is waiting to enter match */
	UPROPERTY(BlueprintReadOnly, replicated, Category = PlayerState)
	uint32 bWaitingPlayer:1;

	/** Whether this player has confirmed ready to play */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 bReadyToPlay:1;

	/** Whether this player has a pending switch team request (waiting for swap partner) */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 bPendingTeamSwitch : 1;

	/** Color to display ready text. */
	FLinearColor ReadyColor;

	/** Last displayed ready state. */
	uint8 LastReadyState;

	/** Color to display ready text. */
	float LastReadySwitchTime;

	/** Color to display ready text. */
	int32 ReadySwitchCount;

	virtual void UpdateReady();

	/** Used for tracking multikills - not always correct as it is reset when player dies. */
	UPROPERTY(BlueprintReadWrite, Category = PlayerState)
	float LastKillTime;

	/** current multikill level (1 = double, 2 = multi, etc)
	 * note that the value isn't reset until a new kill comes in
	 */
	UPROPERTY(BlueprintReadWrite, Category = PlayerState)
	int32 MultiKillLevel;

	/** Current number of consecutive kills without dying. */
	UPROPERTY(Replicated, BlueprintReadWrite, Category = PlayerState)
	int32 Spree;

	/** Kills by this player.  Not replicated but calculated client-side */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	int32 Kills;

	/** Can't respawn once out of lives */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 bOutOfLives:1;

	/** How many times associated player has died */
	UPROPERTY(BlueprintReadOnly, replicated, ReplicatedUsing = OnDeathsReceived, Category = PlayerState)
	int32 Deaths;

	/** How many times has the player captured the flag */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	int32 FlagCaptures;

	/** How many times has the player returned the flag */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	int32 FlagReturns;

	/** How many times has the player captured the flag */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	int32 Assists;

	UPROPERTY(BlueprintReadOnly, replicated, Category = PlayerState)
	AUTPlayerState* LastKillerPlayerState;

	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	bool bIsRconAdmin;

	UPROPERTY(BlueprintReadOnly, replicated, Category = PlayerState)
	bool bHasHighScore;

	// Player Stats 

	/** This is the unique ID for stats generation*/
	FString StatsID;
	
	// How long until this player can respawn.  It's not directly replicated to the clients instead it's set
	// locally via OnDeathsReceived.  It will be set to the value of "GameState.RespawnWaitTime"

	UPROPERTY(BlueprintReadWrite, Category = PlayerState)
	float RespawnTime;

	UPROPERTY(BlueprintReadWrite, Category = PlayerState)
	float ForceRespawnTime;

	UPROPERTY(replicated)
	bool bChosePrimaryRespawnChoice;

	UPROPERTY(replicated)
	class APlayerStart* RespawnChoiceA;

	UPROPERTY(replicated)
	class APlayerStart* RespawnChoiceB;

	/** The currently held object */
	UPROPERTY(BlueprintReadOnly, replicated, ReplicatedUsing = OnCarriedObjectChanged, Category = PlayerState)
	class AUTCarriedObject* CarriedObject;

	/** Last time this player shot the enemy flag carrier. */
	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	float LastShotFCTime;

	/** Enemy flag carrier who was last shot. */
	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	AUTPlayerState* LastShotFC;

	/** Last time this player killed the enemy flag carrier. */
	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	float LastKilledFCTime;

	/** Accumulate partial damage on enemy flag carrier for scoring. */
	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	float FCDamageAccum;

	/** Last time this player returned the flag. */
	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	float LastFlagReturnTime;

	/** Transient - used for triggering assist announcements after a kill. */
	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	bool bNeedsAssistAnnouncement;

	UPROPERTY(BlueprintReadOnly, Category = PlayerState)
	TArray<FWeaponSpree> WeaponSprees;

	UPROPERTY(BlueprintReadOnly, replicated, ReplicatedUsing = OnWeaponSpreeDamage, Category = PlayerState)
	TSubclassOf<UUTDamageType> WeaponSpreeDamage;

	UFUNCTION()
	void OnWeaponSpreeDamage();

	virtual void AnnounceWeaponSpree(int32 SpreeIndex, TSubclassOf<class UUTDamageType> UTDamage);

	UFUNCTION()
	void OnDeathsReceived();

	/** Team has changed, announce, tell pawn, etc. */
	UFUNCTION()
	virtual void HandleTeamChanged(AController* Controller);

	UFUNCTION(BlueprintNativeEvent)
	void NotifyTeamChanged();

	UFUNCTION()
	void OnCarriedObjectChanged();

	UFUNCTION()
	virtual void SetCarriedObject(AUTCarriedObject* NewCarriedObject);

	UFUNCTION()
	virtual void ClearCarriedObject(AUTCarriedObject* OldCarriedObject);


	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void SetWaitingPlayer(bool B);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void IncrementKills(TSubclassOf<UDamageType> DamageType, bool bEnemyKill);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void IncrementDeaths(TSubclassOf<UDamageType> DamageType, AUTPlayerState* KillerPlayerState);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void AdjustScore(int32 ScoreAdjustment);

	virtual void Tick(float DeltaTime) override;

	inline bool IsFemale()
	{
		return false; // TODO
	}

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRequestChangeTeam(uint8 NewTeamIndex);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveStatsID(const FString& NewStatsID);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRecieveCountryFlag(uint32 NewCountryFlag);

	UFUNCTION()
	AUTCharacter* GetUTCharacter();

	UPROPERTY(replicatedUsing = OnRepHat)
	TSubclassOf<AUTHat> HatClass;

	UFUNCTION()
	virtual void OnRepHat();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveHatClass(const FString& NewHatClass);

	UPROPERTY(replicatedUsing = OnRepEyewear)
	TSubclassOf<AUTEyewear> EyewearClass;
	
	UFUNCTION()
	virtual void OnRepEyewear();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveEyewearClass(const FString& NewEyewearClass);

	UPROPERTY(replicatedUsing = OnRepHatVariant)
	int32 HatVariant;

	UFUNCTION()
	virtual void OnRepHatVariant();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveHatVariant(int32 NewVariant);

	UPROPERTY(replicatedUsing = OnRepEyewearVariant)
	int32 EyewearVariant;
	
	UFUNCTION()
	virtual void OnRepEyewearVariant();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveEyewearVariant(int32 NewVariant);

	UPROPERTY(replicated)
	TSubclassOf<AUTTaunt> TauntClass;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveTauntClass(const FString& NewTauntClass);

	UPROPERTY(replicated)
	TSubclassOf<AUTTaunt> Taunt2Class;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveTaunt2Class(const FString& NewTauntClass);

	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

	// Returns the team number of the team that owns this object
	UFUNCTION()
	virtual uint8 GetTeamNum() const;
	// not applicable
	virtual void SetTeamForSideSwap_Implementation(uint8 NewTeamNum) override
	{}

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	virtual	bool ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType);

	// Where should any chat go.  NOTE: some commands like Say and TeamSay ignore this value
	UPROPERTY(replicated)
	FName ChatDestination;

	UFUNCTION(server, reliable, withvalidation)
	virtual void ServerNextChatDestination();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = PlayerState)
	int32 CountryFlag;

	virtual void ValidateEntitlements();

	void WriteStatsToCloud();
	virtual void AddMatchToStats(const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates);

	virtual int32 GetSkillRating(FName SkillStatName);
	virtual void UpdateTeamSkillRating(FName SkillStatName, bool bWonMatch, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates);
	virtual void UpdateIndividualSkillRating(FName SkillStatName, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates);

	/** Cached clamped player name for display. */
	UPROPERTY(BlueprintReadWrite)
	FString ClampedName;

	/** True if clamped name is currently valid. */
	bool bHasValidClampedName;

	/** object which set clamping for my name. */
	UPROPERTY(BlueprintReadWrite)
	UObject* NameClamper;

	virtual void SetPlayerName(const FString& S) override;

	virtual void OnRep_PlayerName();
	
	bool HasWrittenStatsToCloud() { return bWroteStatsToCloud; }

private:
	bool bReadStatsFromCloud;
	bool bSuccessfullyReadStatsFromCloud;
	bool bWroteStatsToCloud;
	int32 DuelSkillRatingThisMatch;
	int32 CTFSkillRatingThisMatch;
	int32 TDMSkillRatingThisMatch;
	int32 DMSkillRatingThisMatch;
	IOnlineIdentityPtr OnlineIdentityInterface;
	IOnlineUserCloudPtr OnlineUserCloudInterface;
	FOnReadUserFileCompleteDelegate OnReadUserFileCompleteDelegate;
	FOnWriteUserFileCompleteDelegate OnWriteUserFileCompleteDelegate;
	FString GetStatsFilename();
	void ReadStatsFromCloud();
	virtual void OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);
	virtual void OnWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);
	
public:
	// Average ELO rank for this player.
	UPROPERTY(Replicated)
	int32 AverageRank;

	// Find the local player and see if we are his friend.
	void OnRep_UniqueId();

	// Calculated client-side by the local player when 
	bool bIsFriend;

#if !UE_SERVER
public:
	const FSlateBrush* GetELOBadgeImage() const;
	const FSlateBrush* GetELOBadgeNumberImage() const;
	void BuildPlayerInfo(TSharedPtr<SVerticalBox> Panel);
#endif

	// If true, the game type considers this player special.
	UPROPERTY(replicatedUsing = OnRepSpecialPlayer)
	uint32 bSpecialPlayer:1;

	UFUNCTION()
	virtual void OnRepSpecialPlayer();


	// Allows gametypes to force a given hat on someone
	UPROPERTY(replicatedUsing = OnRepOverrideHat)
	TSubclassOf<AUTHat> OverrideHatClass;

	UFUNCTION()
	virtual void OnRepOverrideHat();

	virtual void SetOverrideHatClass(const FString& NewOverrideHatClass);

protected:
	UPROPERTY(Replicated)
	float AvailableCurrency;

public:
	UPROPERTY(Replicated)
	TArray<AUTReplicatedLoadoutInfo*> Loadout;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerUpdateLoadout(const TArray<AUTReplicatedLoadoutInfo*>& NewLoadout);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerBuyLoadout(AUTReplicatedLoadoutInfo* DesiredLoadout);

	UFUNCTION(Client, Reliable)
	virtual void ClientShowLoadoutMenu();

	virtual float GetAvailableCurrency();

	virtual void AdjustCurrency(float Adjustment);


protected:
	TArray<FTempBanInfo> BanVotes;

public:
	void LogBanRequest(AUTPlayerState* Voter);
	int CountBanVotes();

	UPROPERTY(Replicated)
	uint8 KickPercent;

};



