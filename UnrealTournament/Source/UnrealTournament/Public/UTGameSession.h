// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGameMode.h"
#include "Online.h"
#include "OnlineBeaconHost.h"
#include "UTServerBeaconHost.h"
#include "UTGameSession.generated.h"


UCLASS(Config=Game)
class UNREALTOURNAMENT_API AUTGameSession : public AGameSession
{
	GENERATED_UCLASS_BODY()

public:
	// Approve a player logging in to the current server
	FString ApproveLogin(const FString& Options);

	// The base engine ApproveLogin doesn't pass the Address and UniqueId to the approve login process.  So we have
	// a second layer.
	virtual void ValidatePlayer(const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage, bool bValidateAsSpectator);

	// Cached reference to the Game Mode
	UPROPERTY()
	AUTBaseGameMode* UTBaseGameMode;

	// Will be true if this server has been registered with the MCP
	bool bSessionValid;

public:		// Online Subsystem stuff

	virtual void Destroyed() override;

	virtual bool ProcessAutoLogin();
	
	virtual void InitOptions( const FString& Options );

	virtual void StartMatch() {}
	virtual void EndMatch() {}

	virtual void UpdateGameState() {}

	virtual void UnRegisterServer(bool bShuttingDown) {}
	
public:
	virtual void HandleMatchHasStarted();
	virtual void HandleMatchHasEnded();

protected:

	virtual void CleanUpOnlineSubsystem();

	FDelegateHandle OnSessionFailuredDelegate;

	// Holds a list of unique ids of admins who can bypass the login checks.
	TArray<FString> AllowedAdmins;
	void SessionFailure(const FUniqueNetId& PlayerId, ESessionFailure::Type ErrorType);

	bool bReregisterWhenDone;

	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	FTimerHandle StartServerTimerHandle;

	virtual void StartServer() {}

public:
	UPROPERTY()
	bool bNoJoinInProgress;

	UPROPERTY(config)
	bool CantBindBeaconPortIsNotFatal;
	
	void AcknowledgeAdmin(const FString& AdminId, bool bIsAdmin);
	void CleanupServerSession();
};