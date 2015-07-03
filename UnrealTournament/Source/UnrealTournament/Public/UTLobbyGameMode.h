// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLobbyPC.h"
#include "UTGameState.h"
#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "../Private/Slate/SUWindowsLobby.h"
#include "UTBaseGameMode.h"
#include "UTServerBeaconLobbyHostListener.h"
#include "UTServerBeaconLobbyHostObject.h"
#include "UTLobbyGameMode.generated.h"

UCLASS(Config = Game)
class UNREALTOURNAMENT_API AUTLobbyGameMode : public AUTBaseGameMode
{
	GENERATED_UCLASS_BODY()

public:
	/** Cached reference to our game state for quick access. */
	UPROPERTY()
	AUTLobbyGameState* UTLobbyGameState;		

	// Once a hub has been alive for this many hours, it will attempt to auto-restart
	// itself when noone is one it.  In HOURS.
	UPROPERTY(GlobalConfig)
	int32 ServerRefreshCheckpoint;

	UPROPERTY(GlobalConfig)
	FString LobbyPassword;

	UPROPERTY(GlobalConfig)
	int32 StartingInstancePort;

	UPROPERTY(GlobalConfig)
	int32 InstancePortStep;

	UPROPERTY(GlobalConfig)
	FString AutoLaunchGameMode;

	UPROPERTY(GlobalConfig)
	FString AutoLaunchGameOptions;

	UPROPERTY(GlobalConfig)
	FString AutoLaunchMap;

	// The Maximum # of instances allowed.  Set to 0 to have no cap 
	UPROPERTY(GlobalConfig)
	int32 MaxInstances;

	/** Minimum number of players that must have joined an instance before it can be started */
	UPROPERTY(GlobalConfig)
	int32 MinPlayersToStart;

	UPROPERTY()
	TSubclassOf<class UUTLocalMessage>  GameMessageClass;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState();
	virtual void StartMatch();
	virtual void RestartPlayer(AController* aPlayer);
	virtual void ChangeName(AController* Other, const FString& S, bool bNameChange);

	
	virtual void PostLogin( APlayerController* NewPlayer );
	virtual FString InitNewPlayer(class APlayerController* NewPlayerController, const TSharedPtr<FUniqueNetId>& UniqueId, const FString& Options, const FString& Portal = TEXT(""));
	virtual void Logout(AController* Exiting);
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player);
	virtual TSubclassOf<AGameSession> GetGameSessionClass() const;
	virtual void OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState);

	virtual bool IsLobbyServer() { return true; }

	virtual void AddInactivePlayer(APlayerState* PlayerState, APlayerController* PC);

	virtual bool IsHandlingReplays();

#if !UE_SERVER

	/**
	 *	Returns the Menu to popup when the user requests a menu
	 **/
	virtual TSharedRef<SUWindowsDesktop> GetGameMenu(UUTLocalPlayer* PlayerOwner) const
	{
		return SNew(SUWindowsLobby).PlayerOwner(PlayerOwner);
	}

#endif
	virtual FName GetNextChatDestination(AUTPlayerState* PlayerState, FName CurrentChatDestination);

protected:

	// The actual instance query port to use.
	int32 InstanceQueryPort;

	/**
	 * Converts a string to a bool.  If the string is empty, it will return the default.
	 **/
	inline bool EvalBoolOptions(FString InOpt, bool Default)
	{
		if (!InOpt.IsEmpty())
		{
			if (FCString::Stricmp(*InOpt,TEXT("True") )==0 
				||	FCString::Stricmp(*InOpt,*GTrue.ToString())==0
				||	FCString::Stricmp(*InOpt,*GYes.ToString())==0)
			{
				return true;
			}
			else if(FCString::Stricmp(*InOpt,TEXT("False"))==0
				||	FCString::Stricmp(*InOpt,*GFalse.ToString())==0
				||	FCString::Stricmp(*InOpt,TEXT("No"))==0
				||	FCString::Stricmp(*InOpt,*GNo.ToString())==0)
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

public:
	virtual void PreLogin(const FString& Options, const FString& Address, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage);
	virtual int32 GetInstanceData(TArray<FGuid>& InstanceIDs);

	virtual int32 GetNumPlayers();
	virtual int32 GetNumMatches();

	// Attempts to make sure the Lobby has the proper information
	virtual void UpdateLobbySession();

	virtual void DefaultTimer();

protected:
	TArray<FString> ParsedMOTD;
};
