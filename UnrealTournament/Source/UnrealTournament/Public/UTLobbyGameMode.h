// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTLobbyPC.h"
#include "UTGameState.h"
#include "UTLobbyGameState.h"
#include "UTLobbyPlayerState.h"
#include "UTLobbyGameMode.generated.h"

namespace ChatDestinations
{
	extern const FName GlobalChat;				// We want to send the chat to everyone
	extern const FName PrivateLocal;			// We want to send this directly to someone in this lobby
	extern const FName CurrentMatch;			// This chat is going just to the current match
	extern const FName Friends;					// We are only sending this to our friends -- NOTE: requires MCP connection

	// TODO: Add instance chat...

}


UCLASS(Config = Game)
class UNREALTOURNAMENT_API AUTLobbyGameMode : public AGameMode
{
	GENERATED_UCLASS_BODY()

public:
	/** Cached reference to our game state for quick access. */
	UPROPERTY()
	AUTLobbyGameState* UTLobbyGameState;		

	UPROPERTY(GlobalConfig)
	FString LobbyPassword;

	UPROPERTY()
	TSubclassOf<class UUTLocalMessage>  GameMessageClass;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState();
	virtual void StartMatch();
	virtual void RestartPlayer(AController* aPlayer);
	virtual void ChangeName(AController* Other, const FString& S, bool bNameChange);

	virtual void GenericPlayerInitialization(AController* C);
	virtual void PostLogin( APlayerController* NewPlayer );
	virtual void Logout(AController* Exiting);
	virtual bool PlayerCanRestart(APlayerController* Player);
	virtual TSubclassOf<AGameSession> GetGameSessionClass() const;
	virtual void OverridePlayerState(APlayerController* PC, APlayerState* OldPlayerState);
protected:
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

	virtual void Destroyed();
};





