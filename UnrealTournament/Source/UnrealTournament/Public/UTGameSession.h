// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGameMode.h"
#include "Online.h"
#include "OnlineBeaconHost.h"
#include "UTServerBeaconHost.h"
#include "UTGameSession.generated.h"

#define EXIT_CODE_FAILED_TRAVEL 1
#define EXIT_CODE_FAILED_LOGIN 2
#define EXIT_CODE_BAD_PLAYLIST 3
#define EXIT_CODE_SESSION_FAILURE 4
#define EXIT_CODE_BEACON_FAILURE 5
#define EXIT_CODE_AUTH_FAILURE 6
#define EXIT_CODE_CONNECTION_FAILURE 7
#define EXIT_CODE_FAILED_UPDATE 8
#define EXIT_CODE_INTERNAL_ERROR 9
#define EXIT_CODE_GRACEFUL 10

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

	virtual void InitOptions(const FString& Options);

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

	/** Maximum number of seconds to play (0 if infinite). Note that the actual server lifetime can be larger due to various reasons. */
	UPROPERTY(Config)
		float ServerSecondsToLive;

	virtual void CheckForPossibleRestart();
	virtual bool ShouldStopServer();
	virtual void Restart() override;
	virtual void ShutdownServer(const FString& Reason, int32 ExitCode = 0);

	virtual void DestroyHostBeacon(bool bPreserveReservations = false) {}

public:
	
	/**
	 * Gracefully shuts down the server the next time it is free.
	 *
	 * @param ExitCode the code to exit the process with
	 */
	void GracefulShutdown(int32 ExitCode);
	
	static bool bGracefulShutdown;

	/** The exit code to exit with when gracefully shutting down */
	static int32 GracefulExitCode;

	UPROPERTY()
	bool bNoJoinInProgress;

	UPROPERTY(config)
	bool CantBindBeaconPortIsNotFatal;
	
	void AcknowledgeAdmin(const FString& AdminId, bool bIsAdmin);
	void CleanupServerSession();
};