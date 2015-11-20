// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "TimerManager.h"
#include "GameNetworkManager.generated.h"

/** Describes which standby detection event occured so the game can take appropriate action. */
UENUM()
enum EStandbyType
{
	STDBY_Rx,
	STDBY_Tx,
	STDBY_BadPing,
	STDBY_MAX,
};


/**
 * Handles game-specific networking management (cheat detection, bandwidth management, etc.).
 */

UCLASS(config=Game, notplaceable)
class ENGINE_API AGameNetworkManager : public AInfo
{
	GENERATED_UCLASS_BODY()

	//======================================================================================================================
	// Listen server dynamic netspeed adjustment
	
	/** Current adjusted net speed - Used for dynamically managing netspeed for listen servers*/
	UPROPERTY()
	int32 AdjustedNetSpeed;

	/**  Last time netspeed was updated for server (by client entering or leaving) */
	UPROPERTY()
	float LastNetSpeedUpdateTime;

	/** Total available bandwidth for listen server, split dynamically across net connections */
	UPROPERTY(globalconfig)
	int32 TotalNetBandwidth;

	/** Minimum bandwidth dynamically set per connection */
	UPROPERTY(globalconfig)
	int32 MinDynamicBandwidth;

	/** Maximum bandwidth dynamically set per connection */
	UPROPERTY(globalconfig)
	int32 MaxDynamicBandwidth;

	//======================================================================================================================
	// Standby cheat detection
	
	/** Used to determine if checking for standby cheats should occur */
	UPROPERTY(config)
	uint32 bIsStandbyCheckingEnabled:1;

	/** Used to determine whether we've already caught a cheat or not */
	UPROPERTY()
	uint32 bHasStandbyCheatTriggered:1;

	/** The amount of time without packets before triggering the cheat code */
	UPROPERTY(config)
	float StandbyRxCheatTime;

	/** The amount of time without packets before triggering the cheat code */
	UPROPERTY(config)
	float StandbyTxCheatTime;

	/** The point we determine the server is either delaying packets or has bad upstream */
	UPROPERTY(config)
	int32 BadPingThreshold;

	/** The percentage of clients missing RX data before triggering the standby code */
	UPROPERTY(config)
	float PercentMissingForRxStandby;

	/** The percentage of clients missing TX data before triggering the standby code */
	UPROPERTY(config)
	float PercentMissingForTxStandby;

	/** The percentage of clients with bad ping before triggering the standby code */
	UPROPERTY(config)
	float PercentForBadPing;

	/** The amount of time to wait before checking a connection for standby issues */
	UPROPERTY(config)
	float JoinInProgressStandbyWaitTime;

	//======================================================================================================================
	// Player replication
	
	/** Average size of replicated move packet (ServerMove() packet size) from player */
	UPROPERTY()
	float MoveRepSize;

	/** MAXPOSITIONERRORSQUARED is the square of the max position error that is accepted (not corrected) in net play */
	UPROPERTY(globalconfig)
	float MAXPOSITIONERRORSQUARED;

	/** MAXNEARZEROVELOCITYSQUARED is the square of the max velocity that is considered zero (not corrected) in net play */
	UPROPERTY()
	float MAXNEARZEROVELOCITYSQUARED;

	/** CLIENTADJUSTUPDATECOST is the bandwidth cost in bytes of sending a client adjustment update. 180 is greater than the actual cost, but represents a tweaked value reserving enough bandwidth for
	other updates sent to the client.  Increase this value to reduce client adjustment update frequency, or if the amount of data sent in the clientadjustment() call increases */
	UPROPERTY()
	float CLIENTADJUSTUPDATECOST;

	/** MAXCLIENTUPDATEINTERVAL is the maximum time between movement updates from the client before the server forces an update. */
	UPROPERTY()
	float MAXCLIENTUPDATEINTERVAL;

	/** If client update is within MAXPOSITIONERRORSQUARED then he is authorative on his final position */
	UPROPERTY(globalconfig)
	bool ClientAuthorativePosition;

	/**  Update network speeds for listen servers based on number of connected players.  */
	virtual void UpdateNetSpeeds(bool bIsLanMatch);

	/** Timer which calls UpdateNetSpeeds() once a second. */
	virtual void UpdateNetSpeedsTimer();

	//======================================================================================================================
	// Player replication

	/** @return true if last player client to server update was sufficiently recent.  Used to limit frequency of corrections if connection speed is limited. */
	virtual bool WithinUpdateDelayBounds(class APlayerController* PC, float LastUpdateTime) const;

	/** @return true if position error exceeds max allowable amount */
	virtual bool ExceedsAllowablePositionError(FVector LocDiff) const;

	/** @return true if velocity vector passed in is considered near zero for networking purposes */
	virtual bool NetworkVelocityNearZero(FVector InVelocity) const;
	virtual void PostInitializeComponents() override;

	/** @RETURN new per/client bandwidth given number of players in the game */
	virtual int32 CalculatedNetSpeed();
	
	//======================================================================================================================
	// Standby cheat detection
	/**
	 * Turns standby detection on/off
	 * @param bIsEnabled true to turn it on, false to disable it
	 */
	virtual void EnableStandbyCheatDetection(bool bIsEnabled);

	/**
	 * Notifies the game code that a standby cheat was detected
	 * @param StandbyType the type of cheat detected
	 */
	virtual void StandbyCheatDetected(EStandbyType StandbyType);

	/** If true, actor network relevancy is constrained by whether they are within their NetCullDistanceSquared from the client's view point. */
	UPROPERTY(globalconfig)
	bool	bUseDistanceBasedRelevancy;

protected:

	/** Handle for efficient management of UpdateNetSpeeds timer */
	FTimerHandle TimerHandle_UpdateNetSpeedsTimer;	
};



