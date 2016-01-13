// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGameMode.h"
#include "Online.h"
#include "OnlineBeaconHost.h"
#include "UTServerBeaconHost.h"
#include "UTGameSession.generated.h"

const float SERVER_REREGISTER_WAIT_TIME=120.0;  // 2 minutes

USTRUCT()
struct FBanInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString UserName;

	UPROPERTY()
	FString UniqueID;

	FBanInfo()
		: UserName(TEXT(""))
		, UniqueID(TEXT(""))
	{
	}

	FBanInfo(const FString& inUserName, const FString& inUniqueID)
		: UserName(inUserName)
		, UniqueID(inUniqueID)
	{
	}

};

UCLASS(Config=Game)
class UNREALTOURNAMENT_API AUTGameSession : public AGameSession
{
	GENERATED_UCLASS_BODY()

public:
	// Approve a player logging in to the current server
	FString ApproveLogin(const FString& Options);

	// The base engine ApproveLogin doesn't pass the Address and UniqueId to the approve login process.  So we have
	// a second layer.
	void ValidatePlayer(const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage, bool bValidateAsSpectator);

	// Cached reference to the Game Mode
	UPROPERTY()
	AUTBaseGameMode* UTBaseGameMode;

	// Will be true if this server has been registered with the MCP
	bool bSessionValid;

public:		// Online Subsystem stuff

	// Make sure to clean everything up
	virtual void Destroyed();
	
	virtual bool ProcessAutoLogin();
	
	virtual void InitOptions( const FString& Options );
	virtual void RegisterServer();
	virtual void UnRegisterServer(bool bShuttingDown);

	virtual void StartMatch();
	virtual void EndMatch();
	virtual void CleanUpOnlineSubsystem();

	virtual void UpdateGameState();

	void InitHostBeacon(FOnlineSessionSettings* SessionSettings);
	void DestroyHostBeacon();

protected:

	/** General beacon listener for registering beacons with */
	AOnlineBeaconHost* BeaconHostListener;
	/** Beacon controlling access to this game */
	UPROPERTY(Transient)
	AUTServerBeaconHost* BeaconHost;

	FDelegateHandle OnCreateSessionCompleteDelegate;
	FDelegateHandle OnStartSessionCompleteDelegate;
	FDelegateHandle OnEndSessionCompleteDelegate;
	FDelegateHandle OnDestroySessionCompleteDelegate;
	FDelegateHandle OnUpdateSessionCompleteDelegate;
	FDelegateHandle OnSessionFailuredDelegate;
	FDelegateHandle OnConnectionStatusDelegate;
	
	virtual void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);

public:
	virtual bool BanPlayer(APlayerController* BannedPlayer, const FText& BanReason);
	virtual void HandleMatchHasStarted();
	virtual void HandleMatchHasEnded();
	virtual bool KickPlayer(APlayerController* KickedPlayer, const FText& KickReason) override;

protected:
	UPROPERTY(Config)
	TArray<FBanInfo> BannedUsers;

	// Users in this array are not saved.  It's used when kicking a player from an instance.  They don't get to come back.
	UPROPERTY()
	TArray<FBanInfo> InstanceBannedUsers;

	// Holds a list of unique ids of admins who can bypass the login checks.
	TArray<FString> AllowedAdmins;
	void SessionFailure(const FUniqueNetId& PlayerId, ESessionFailure::Type ErrorType);
	void OnConnectionStatusChanged(EOnlineServerConnectionStatus::Type LastConnectionState, EOnlineServerConnectionStatus::Type ConnectionState);

	bool bReregisterWhenDone;

public:
	UPROPERTY()
	bool bNoJoinInProgress;

	UPROPERTY(config)
	bool CantBindBeaconPortIsNotFatal;
	
	void AcknowledgeAdmin(const FString& AdminId, bool bIsAdmin);
};