// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTTeamInterface.h"
#include "Online.h"
#include "OnlineSubsystemTypes.h"
#include "UTDamageType.h"
#include "UTHat.h"
#include "UTHatLeader.h"
#include "UTEyewear.h"
#include "UTTaunt.h"
#include "Http.h"
#include "UTProfileItem.h"
#include "SHyperlink.h"
#include "UTCharacterVoice.h"
#include "StatManager.h"
#include "UTWeaponSkin.h"
#include "OnlineNotification.h"
#if WITH_PROFILE
#include "UtMcpProfile.h"
#endif

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
	TSharedPtr<const FUniqueNetId> Voter;

	// When was the vote cast.  Votes will time out over time (5mins)
	float BanTime;

	FTempBanInfo(const TSharedPtr<const FUniqueNetId> inVoter, float inBanTime)
		: Voter(inVoter)
		, BanTime(inBanTime)
	{
	}

};

USTRUCT()
struct FEmoteRepInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	uint8 EmoteCount;

	UPROPERTY()
	int32 EmoteIndex;
};

class AUTReplicatedMapInfo;
class AUTRconAdminInfo;

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

	UFUNCTION(BlueprintCallable, Category = Character)
		virtual void SetCharacterVoice(const FString& CharacterVoicePath);

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

	/** Whether this spectator is a caster */
	UPROPERTY(replicated)
	uint32 bCaster : 1;

	/** Whether this player has a pending switch team request (waiting for swap partner) */
	UPROPERTY(BlueprintReadWrite, replicated, Category = PlayerState)
	uint32 bPendingTeamSwitch : 1;

	/** Persistent so deathmessage can know about it. */
	UPROPERTY()
		uint32 bAnnounceWeaponSpree : 1;

	/** Persistent so deathmessage can know about it. */
	UPROPERTY()
		uint32 bAnnounceWeaponReward : 1;

	/** Color to display ready text. */
	FLinearColor ReadyColor;

	/** Scale to display ready text. */
	float ReadyScale;

	/** Last displayed ready state. */
	uint8 LastReadyState;

	/** Last Ready state change time. */
	float LastReadySwitchTime;

	/** Count of fast ready state changes. */
	int32 ReadySwitchCount;

	UFUNCTION(BlueprintCallable, Category = PlayerState)
	virtual void UpdateReady();

	/** Voice used by this player/bot for speech (taunts, etc.). */
	UPROPERTY(BlueprintReadOnly, Category = Sounds)
		TSubclassOf<class UUTCharacterVoice> CharacterVoice;

	UPROPERTY()
		bool bShouldAutoTaunt;

	/** Last time this player sent a taunt voice message. */
	UPROPERTY()
		float LastTauntTime;

	FTimerHandle PlayKillAnnouncement;
	FTimerHandle UpdateOldNameHandle;

	/** Delay oldname update so it's available for name change notification. */
	virtual void UpdateOldName();

	/** Whether this player plays auto-taunts. */
	virtual bool ShouldAutoTaunt() const;

	virtual void AnnounceKill();

	virtual void AnnounceSameTeam(class AUTPlayerController* ShooterPC);

	virtual void AnnounceReactionTo(const AUTPlayerState* ReactionPS) const;

	virtual void AnnounceStatus(FName NewStatus);

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

	UPROPERTY(BlueprintReadOnly, Category = PlayerState, replicated)
	bool bIsRconAdmin;

	UPROPERTY(BlueprintReadOnly, replicated, Category = PlayerState)
	bool bHasHighScore;

	UPROPERTY(BlueprintReadOnly, replicated, Category = PlayerState)
	bool bIsDemoRecording;

	UPROPERTY()
	bool bAllowedEarlyLeave;

	// Player Stats 

	/** This is the unique ID for stats generation*/
	UPROPERTY(replicated)
	FString StatsID;
	
	/** Add an entry to MatchHighlights only if an empty slot is found. */
	virtual void AddMatchHighlight(FName NewHighlight, float HighlightData);

	/** Set at end of match and half-time. */
	UPROPERTY(replicated)
		FName MatchHighlights[5];

	/** Set at end of match and half-time. */
	UPROPERTY(replicated)
		float MatchHighlightData[5];

	/** Set at end of match and half-time. */
	UPROPERTY(replicated)
		TSubclassOf<class AUTWeapon> FavoriteWeapon;

	UPROPERTY()
	int32 ElapsedTime;

protected:
	/** XP player had before current match, read from backend (-1 until successful read) */
	UPROPERTY(replicated)
	int32 PrevXP;

	/** XP awarded to this player so far (server only, replicated to owning client via RPC after end of game) */
	UPROPERTY()
	FXPBreakdown XP;
public:
	/** Currently awarded challenge stars. */
	UPROPERTY(replicated)
	int32 TotalChallengeStars;

	inline int32 GetPrevXP() const
	{
		return PrevXP;
	}
	inline const FXPBreakdown& GetXP() const
	{
		return XP;
	}
	inline bool CanAwardOnlineXP() const
	{
		return PrevXP >= 0 && UniqueId.IsValid() && !StatsID.IsEmpty(); // PrevXP == -1 before a successful read, should always be >= 0 after even if there was no value
	}

	void GiveXP(const FXPBreakdown& AddXP);
	void ClampXP(int32 MaxValue);
	void ApplyBotXPPenalty(float GameDifficulty)
	{
		XP = XP * (0.4f + FMath::Clamp(GameDifficulty, 0.f, 7.f) * 0.05f);
	}

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

	/** used for gametypes where players make a choice in order (e.g. Showdown spawn selection) to indicate selection sequence */
	UPROPERTY(Replicated, BlueprintReadOnly)
	uint8 SelectionOrder;

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

	virtual void AnnounceWeaponSpree(TSubclassOf<class UUTDamageType> UTDamage);

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
	virtual void IncrementKills(TSubclassOf<UDamageType> DamageType, bool bEnemyKill, AUTPlayerState* VictimPS=NULL);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void IncrementDeaths(TSubclassOf<UDamageType> DamageType, AUTPlayerState* KillerPlayerState);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = PlayerState)
	virtual void AdjustScore(int32 ScoreAdjustment);

	virtual void Tick(float DeltaTime) override;

	/** Return true if character is female. */
	virtual bool IsFemale();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerRequestChangeTeam(uint8 NewTeamIndex);

	UFUNCTION()
	AUTCharacter* GetUTCharacter();

	UPROPERTY(replicated)
	TArray<UUTWeaponSkin*> WeaponSkins;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveWeaponSkin(const FString& NewWeaponSkin);
	
	UFUNCTION()
	virtual void UpdateWeaponSkinPrefFromProfile(TSubclassOf<AUTWeapon> Weapon);

	UPROPERTY(replicatedUsing = OnRepHat)
	TSubclassOf<AUTHat> HatClass;

	UFUNCTION()
	virtual void OnRepHat();

	UPROPERTY(replicatedUsing = OnRepHatLeader)
	TSubclassOf<AUTHatLeader> LeaderHatClass;

	UFUNCTION()
	virtual void OnRepHatLeader();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveHatClass(const FString& NewHatClass);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerReceiveLeaderHatClass(const FString& NewLeaderHatClass);

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

	virtual bool ShouldBroadCastWelcomeMessage(bool bExiting = false) override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	virtual	bool ModifyStat(FName StatName, int32 Amount, EStatMod::Type ModType);

	// Where should any chat go.  NOTE: some commands like Say and TeamSay ignore this value
	UPROPERTY(replicated)
	FName ChatDestination;

	UFUNCTION(server, reliable, withvalidation)
	virtual void ServerNextChatDestination();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = PlayerState)
	FName CountryFlag;

	virtual void Destroyed() override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = PlayerState)
	FName Avatar;

private:
	UPROPERTY()
	UObject* McpProfile;

public:
#if WITH_PROFILE
	inline UUtMcpProfile* GetMcpProfile() const
	{
		checkSlow(Cast<UUtMcpProfile>(McpProfile) != NULL);
		return Cast<UUtMcpProfile>(McpProfile);
	}
	// allows UTBaseGameMode to be the only class that can set McpProfile
	friend struct FMcpProfileSetter;
	struct FMcpProfileSetter
	{
		friend class AUTBaseGameMode;
	private:
		static void Set(AUTPlayerState* PS, UUtMcpProfile* Profile)
		{
			PS->McpProfile = Profile;
			Profile->OnStatsUpdated().AddUObject(PS, &AUTPlayerState::ProfileStatsChanged);
			Profile->OnHandleNotification().BindUObject(PS, &AUTPlayerState::ProfileNotification);
			Profile->OnInventoryUpdated().AddUObject(PS, &AUTPlayerState::ProfileItemsChanged);
			if (Profile->HasValidProfileData())
			{
				PS->ProfileStatsChanged(0);
				PS->ProfileItemsChanged(TSet<FString>(), 0);
			}
		}
	};
#endif
	
	/** temp while backend sends profile notifications to server when server triggered the update instead of client like it should */
	void ProfileNotification(const FOnlineNotification& Notification);

	void ProfileStatsChanged(int64 ProfileRevision)
	{
#if WITH_PROFILE
		if (PrevXP == GetClass()->GetDefaultObject<AUTPlayerState>()->PrevXP)
		{
			PrevXP = GetMcpProfile()->GetXP();
		}
#endif
	}
	void ProfileItemsChanged(const TSet<FString>& ChangedTypes, int64 ProfileRevision);

	inline bool IsProfileItemListPending() const
	{
#if WITH_PROFILE
		return McpProfile == NULL || !GetMcpProfile()->HasValidProfileData();
#else
		return false;
#endif
	}
	inline bool OwnsItem(const UUTProfileItem* Item) const
	{
#if WITH_PROFILE
		TSharedPtr<const FMcpItem> McpItem = GetMcpProfile()->FindItemByTemplate(Item->GetTemplateID());
		return (McpItem.IsValid() && McpItem->Quantity > 0);
#else
		return false;
#endif
	}
	/** returns whether the user owns an item that grants the asset (cosmetic, character, whatever) with the given path */
	inline bool OwnsItemFor(const FString& Path, int32 VariantId = 0) const;

protected:
	/** returns whether this user has rights to the given item
	 * assumes entitlement and item list queries are completed
	 */
	virtual bool HasRightsFor(UObject* Obj) const;
public:
	virtual void ValidateEntitlements();
	virtual bool ValidateEntitlementSingleObject(UObject* Object);

	void WriteStatsToCloud();
	void StatsWriteComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);
	virtual void AddMatchToStats(const FString& MapName, const FString& GameType, const TArray<class AUTTeamInfo*>* Teams, const TArray<APlayerState*>* ActivePlayerStates, const TArray<APlayerState*>* InactivePlayerStates);

	int32 BotELOLimit;
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

	void ReadStatsFromCloud();

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
	virtual void OnReadUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);
	virtual void OnWriteUserFileComplete(bool bWasSuccessful, const FUniqueNetId& InUserId, const FString& FileName);

	/** map of additional stats used for scoring display. */
	TMap< FName, float > StatsData;

public:
	/** Last time StatsData was updated - used when replicating the data. */
	UPROPERTY()
		float LastScoreStatsUpdateTime;

	/** Accessors for StatsData. */
	float GetStatsValue(FName StatsName) const;
	void SetStatsValue(FName StatsName, float NewValue);
	void ModifyStatsValue(FName StatsName, float Change);

	// Average ELO rank for this player.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = PlayerState)
	int32 AverageRank;
	UPROPERTY(Replicated)
	int32 DuelRank;
	UPROPERTY(Replicated)
	int32 CTFRank;
	UPROPERTY(Replicated)
	int32 TDMRank;
	UPROPERTY(Replicated)
	int32 DMRank;

	UPROPERTY(Replicated)
	int32 TrainingLevel;

#if !UE_SERVER
	FText GetTrainingLevelText();
#endif

	// transient, for TeamInfo leader updates - persistent value is stored in StatsData
	float AttackerScore;
	float SecondaryAttackerScore;
	float DefenderScore;
	float SupporterScore;

	// Find the local player and see if we are his friend.
	void OnRep_UniqueId();

	virtual void RegisterPlayerWithSession(bool bWasFromInvite) override;
	virtual void UnregisterPlayerWithSession() override;

	// Calculated client-side by the local player when 
	bool bIsFriend;

#if !UE_SERVER
public:
	const FSlateBrush* GetELOBadgeImage(int32 EloRating, bool bSmall = false) const;
	const FSlateBrush* GetELOBadgeNumberImage(int32 EloRating) const;
	void BuildPlayerInfo(TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<struct TAttributeStat> >& StatList);
	TSharedRef<SWidget> BuildRankInfo();
	TSharedRef<SWidget> BuildStatsInfo();
	TSharedRef<SWidget> BuildRank(FText RankName, int32 Rank);
	void EpicIDClicked();
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
	int32 CountBanVotes();

	UPROPERTY(Replicated)
	uint8 KickPercent;

	UFUNCTION(server, reliable, withvalidation)
	virtual void RegisterVote(AUTReplicatedMapInfo* VoteInfo);

	virtual void OnRep_bIsInactive() override;

	UPROPERTY(replicatedUsing = OnRepTaunt)
	FEmoteRepInfo EmoteReplicationInfo;

	UFUNCTION()
	virtual void OnRepTaunt();

	UPROPERTY(replicatedUsing = OnRepEmoteSpeed)
	float EmoteSpeed;

	UFUNCTION()
	virtual void OnRepEmoteSpeed();

	UFUNCTION(BlueprintCallable, Category = Taunt)
	void PlayTauntByIndex(int32 TauntIndex);

	UFUNCTION(BlueprintCallable, Category = Taunt)
	void PlayTauntByClass(TSubclassOf<AUTTaunt> TauntToPlay);

	/** whether the player is allowed to freeze a taunt anim; i.e. set its playrate to zero */
	virtual bool AllowFreezingTaunts() const;

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSetEmoteSpeed(float NewEmoteSpeed);

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerFasterEmote();

	UFUNCTION(Server, Reliable, WithValidation)
	virtual void ServerSlowerEmote();

	/** Transient, used to sort players */
	UPROPERTY()
		float MatchHighlightScore;

	UFUNCTION(Client, Reliable)
	virtual void ClientReceiveRconMessage(const FString& Message);

};



