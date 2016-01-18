// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DemoNetConnection.generated.h"


struct FQueuedDemoPacket
{
	TArray< uint8 > Data;
};


/**
 * Simulated network connection for recording and playing back game sessions.
 */
UCLASS(transient, config=Engine)
class UDemoNetConnection
	: public UNetConnection
{
	GENERATED_UCLASS_BODY()

public:

	// UNetConnection interface.

	virtual void InitConnection( class UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed = 0, int32 InMaxPacket=0) override;
	virtual FString LowLevelGetRemoteAddress( bool bAppendPort = false ) override;
	virtual FString LowLevelDescribe() override;
	virtual void LowLevelSend( void* Data, int32 Count ) override;
	virtual int32 IsNetReady( bool Saturate ) override;
	virtual void FlushNet( bool bIgnoreSimulation = false ) override;
	virtual void HandleClientPlayer( APlayerController* PC, class UNetConnection* NetConnection ) override;
	virtual bool ClientHasInitializedLevelFor( const UObject* TestObject ) const override;

public:

	/** @return The DemoRecording driver object */
	FORCEINLINE class UDemoNetDriver* GetDriver()
	{
		return (UDemoNetDriver*)Driver;
	}

	/** @return The DemoRecording driver object */
	FORCEINLINE const class UDemoNetDriver* GetDriver() const
	{
		return (UDemoNetDriver*)Driver;
	}

	TArray<FQueuedDemoPacket> QueuedDemoPackets;

private:
	void TrackSendForProfiler(const void* Data, int32 NumBytes);
};
