// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
	void ValidatePlayer(const FString& Address, const TSharedPtr<class FUniqueNetId>& UniqueId, FString& ErrorMessage);

	// Cached reference to the Game Mode
	AUTBaseGameMode* UTGameMode;
	
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
	
	virtual void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);

public:
	virtual bool BanPlayer(APlayerController* BannedPlayer, const FText& BanReason);

protected:
	UPROPERTY(Config)
	TArray<FString> BannedUsers;

};