// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameMode.h"
#include "UTPlayerState.h"
#include "Private/Slate/SUTInGameMenu.h"
#include "UTBaseGameMode.generated.h"

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

#if !UE_SERVER

	/**
	 *	Returns the Menu to popup when the user requests a menu
	 **/
	virtual TSharedRef<SUWindowsDesktop> GetGameMenu(UUTLocalPlayer* PlayerOwner) const
	{
		return SNew(SUTInGameMenu).PlayerOwner(PlayerOwner);
	}

#endif

protected:

	/** Handle for efficient management of DefaultTimer timer */
	FTimerHandle TimerHandle_DefaultTimer;

	// Will be > 0 if this is an instance created by lobby
	uint32 LobbyInstanceID;

public:

	/** human readable localized name for the game mode */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category = Game)
	FText DisplayName;

	virtual void PreLogin(const FString& Options, const FString& Address, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage) override;
	virtual APlayerController* Login(class UPlayer* NewPlayer, ENetRole RemoteRole, const FString& Portal, const FString& Options, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void GenericPlayerInitialization(AController* C);
	
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	// The Unique ID for this game instance.
	FGuid ServerInstanceGUID;

	virtual bool IsGameInstanceServer() { return LobbyInstanceID > 0; }
	virtual bool IsLobbyServer() { return false; }

	virtual FName GetNextChatDestination(AUTPlayerState* PlayerState, FName CurrentChatDestination);

	// Returns the # of instances controlled by this game mode and fills out the HostNames and Descriptions arrays.  
	virtual int32 GetInstanceData(TArray<FGuid>& InstanceIDs);

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

	virtual bool FindRedirect(const FString& PackageName, FPackageRedirectReference& Redirect);
	virtual FString GetRedirectURL(const FString& PackageName) const;
private:
	FString GetCloudID() const;
};