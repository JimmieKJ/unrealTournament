// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGameMode.h"
#include "Online.h"
#include "UTGameSession.generated.h"

UCLASS()
class AUTGameSession : public AGameSession
{
	GENERATED_UCLASS_BODY()

public:
	// Approve a player logging in to the current server
	FString ApproveLogin(const FString& Options);

	// Cached reference to the Game Mode
	AUTGameMode* UTGameMode;
	
public:		// Online Subsystem stuff

	// Make sure to clean everything up
	virtual void Destroyed();
	
	virtual bool ProcessAutoLogin();
	
	virtual void InitOptions( const FString& Options );
	virtual void RegisterServer();
	virtual void UnRegisterServer();

	virtual void StartMatch();
	virtual void EndMatch();
	virtual void CleanUpOnlineSubsystem();

	virtual void UpdateGameState();

protected:

	FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate;
	FOnStartSessionCompleteDelegate OnStartSessionCompleteDelegate;
	FOnEndSessionCompleteDelegate OnEndSessionCompleteDelegate;
	FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate;
	
	
	virtual void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

};