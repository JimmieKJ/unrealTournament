// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UTGameSessionNonRanked.generated.h"

const float SERVER_REREGISTER_WAIT_TIME = 120.0;  // 2 minutes

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

UCLASS(Config = Game)
class UNREALTOURNAMENT_API AUTGameSessionNonRanked : public AUTGameSession
{
	GENERATED_BODY()

public:
	virtual void RegisterServer() override;
	virtual void UnRegisterServer(bool bShuttingDown) override;

	virtual void ValidatePlayer(const FString& Address, const TSharedPtr<const FUniqueNetId>& UniqueId, FString& ErrorMessage, bool bValidateAsSpectator) override;
public:

	void DestroyHostBeacon();

	virtual void UpdateGameState() override;

	virtual bool KickPlayer(APlayerController* KickedPlayer, const FText& KickReason) override;
	virtual bool BanPlayer(APlayerController* BannedPlayer, const FText& BanReason) override;

	virtual void StartMatch() override;
	virtual void EndMatch() override;
	virtual void CleanUpOnlineSubsystem();

protected:

	FDelegateHandle OnCreateSessionCompleteDelegate;
	FDelegateHandle OnStartSessionCompleteDelegate;
	FDelegateHandle OnEndSessionCompleteDelegate;
	FDelegateHandle OnDestroySessionCompleteDelegate;
	FDelegateHandle OnUpdateSessionCompleteDelegate;
	FDelegateHandle OnConnectionStatusDelegate;

	virtual void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	virtual void OnConnectionStatusChanged(EOnlineServerConnectionStatus::Type LastConnectionState, EOnlineServerConnectionStatus::Type ConnectionState);

protected:

	void InitHostBeacon(FOnlineSessionSettings* SessionSettings);

	/** General beacon listener for registering beacons with */
	AOnlineBeaconHost* BeaconHostListener;
	/** Beacon controlling access to this game */
	UPROPERTY(Transient)
	AUTServerBeaconHost* BeaconHost;
	
	UPROPERTY(Config)
	TArray<FBanInfo> BannedUsers;

	// Users in this array are not saved.  It's used when kicking a player from an instance.  They don't get to come back.
	UPROPERTY()
	TArray<FBanInfo> InstanceBannedUsers;
};