// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameMode.h"
#include "UTPlayerState.h"
#if WITH_PROFILE
#include "UtMcpProfileManager.h"
#endif
#include "UTBaseGameMode.generated.h"

#if !UE_SERVER
	class SUTMenuBase;
#endif

class UUTLocalPlayer;

UCLASS()
class UNREALTOURNAMENT_API AUTBaseGameMode : public AGameMode
{
	GENERATED_UCLASS_BODY()

public:
	virtual void PreInitializeComponents() override;
	virtual void DefaultTimer() { };

	//Password required to join as a player
	UPROPERTY(GlobalConfig)
	FString ServerPassword;

	//Password required to join as a spectator
	UPROPERTY(GlobalConfig)
	FString SpectatePassword;

	uint32 bRequirePassword:1;

	// note that this is not visible because in the editor we will still have the user set DefaultPawnClass, see PostEditChangeProperty()
	// in C++ constructors, you should set this as it takes priority in InitGame()
	UPROPERTY()
	TAssetSubclassOf<APawn> PlayerPawnObject;

#if !UE_SERVER

	/**
	 *	Returns the Menu to popup when the user requests a menu
	 **/
	virtual TSharedRef<SUTMenuBase> GetGameMenu(UUTLocalPlayer* PlayerOwner) const;

#endif

protected:

	/** Handle for efficient management of DefaultTimer timer */
	FTimerHandle TimerHandle_DefaultTimer;

	// Will be > 0 if this is an instance created by lobby
	uint32 LobbyInstanceID;

	// Creates and stores a new server ID
	void CreateServerID();

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		if (DefaultPawnClass != NULL && PropertyChangedEvent.Property != NULL && PropertyChangedEvent.Property->GetFName() == FName(TEXT("DefaultPawnClass")))
		{
			PlayerPawnObject = DefaultPawnClass;
		}
		Super::PostEditChangeProperty(PropertyChangedEvent);
	}
#endif

	/** human readable localized name for the game mode */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = Game)
	FText DisplayName;

	virtual void PreLogin(const FString& Options, const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage) override;
	virtual APlayerController* Login(class UPlayer* NewPlayer, ENetRole RemoteRole, const FString& Portal, const FString& Options, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void GenericPlayerInitialization(AController* C);
	
	virtual void PostInitProperties();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// Holds the server instance guid.  This is created when 
	UPROPERTY(GlobalConfig)
	FString ServerInstanceID;

	// The Unique ID for this game instance.
	FGuid ServerInstanceGUID;

	virtual bool IsGameInstanceServer() { return LobbyInstanceID > 0; }
	virtual bool IsLobbyServer() { return false; }

	virtual FName GetNextChatDestination(AUTPlayerState* PlayerState, FName CurrentChatDestination);

	// Returns the # of instances controlled by this game mode and fills out the HostNames and Descriptions arrays.  
	virtual void GetInstanceData(TArray<TSharedPtr<FServerInstanceData>>& InstanceData);

	// Returns the # of players in this game.  By Default returns NumPlayers but can be overrride in children (like the HUBs)
	virtual int32 GetNumPlayers();

	// Returns the # of matches assoicated with this game type.  Typical returns 1 (this match) but HUBs will return all of their active matches
	virtual int32 GetNumMatches();

	// The Minimum ELO rank allowed on this server.
	UPROPERTY(Config)
	int32 MinAllowedRank;

	UPROPERTY(Config)
	int32 MaxAllowedRank;

	UPROPERTY(Config)
	bool bTrainingGround;

	/**
	 * Converts a string to a bool.  If the string is empty, it will return the default.
	 **/
	static inline bool EvalBoolOptions(const FString& InOpt, bool Default)
	{
		if (!InOpt.IsEmpty())
		{
			if (FCString::Stricmp(*InOpt, TEXT("True")) == 0
				|| FCString::Stricmp(*InOpt, *GTrue.ToString()) == 0
				|| FCString::Stricmp(*InOpt, *GYes.ToString()) == 0)
			{
				return true;
			}
			else if (FCString::Stricmp(*InOpt, TEXT("False")) == 0
				|| FCString::Stricmp(*InOpt, *GFalse.ToString()) == 0
				|| FCString::Stricmp(*InOpt, TEXT("No")) == 0
				|| FCString::Stricmp(*InOpt, *GNo.ToString()) == 0)
			{
				return false;
			}
			else
			{
				return FCString::Atoi(*InOpt) != 0;
			}
		}
		else
		{
			return Default;
		}
	}

	UPROPERTY(Config)
	TArray<FPackageRedirectReference> RedirectReferences;

	virtual void GatherRequiredRedirects(TArray<FPackageRedirectReference>& Redirects);
	virtual bool FindRedirect(const FString& PackageName, FPackageRedirectReference& Redirect);
	virtual void GameWelcomePlayer(UNetConnection* Connection, FString& RedirectURL) override;
private:
	FString GetCloudID() const;

	// Never allow the engine to start replays, we'll do it manually
	bool IsHandlingReplays() override { return false; }

public:
	virtual void BuildPlayerInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<struct TAttributeStat> >& StatList){};
	virtual void BuildScoreInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<struct TAttributeStat> >& StatList){};
	virtual void BuildRewardInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<struct TAttributeStat> >& StatList){};
	virtual void BuildWeaponInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<struct TAttributeStat> >& StatList){};
	virtual void BuildMovementInfo(AUTPlayerState* PlayerState, TSharedPtr<class SUTTabWidget> TabWidget, TArray<TSharedPtr<struct TAttributeStat> >& StatList){};

	virtual void SendRconMessage(const FString& DestinationId, const FString &Message);

	// Kicks a player
	virtual void RconKick(const FString& NameOrUIDStr, bool bBan, const FString& Reason);
	virtual void RconAuth(AUTBasePlayerController* Admin, const FString& Password);
	virtual void RconNormal(AUTBasePlayerController* Admin);

private:
	UPROPERTY()
	UObject* McpProfileManager;
public:
#if WITH_PROFILE
	inline UUtMcpProfileManager* GetMcpProfileManager() const
	{
		checkSlow(Cast<UUtMcpProfileManager>(McpProfileManager) != NULL);
		return Cast<UUtMcpProfileManager>(McpProfileManager);
	}
#endif

	// This will be set to true if this is a private match.  
	UPROPERTY()
	bool bPrivateMatch;
	
};