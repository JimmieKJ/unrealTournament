// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//
// Simulated network connection for recording and playing back game sessions.
//

#pragma once
#include "DemoNetConnection.generated.h"

struct FQueuedDemoPacket
{
	TArray< uint8 > Data;
};

UCLASS(transient, config=Engine)
class UDemoNetConnection : public UNetConnection
{
	GENERATED_UCLASS_BODY()

	// Begin UNetConnection interface.
	virtual void		InitConnection( class UNetDriver* InDriver, EConnectionState InState, const FURL& InURL, int32 InConnectionSpeed = 0 ) override;
	virtual FString		LowLevelGetRemoteAddress( bool bAppendPort=false ) override;
	virtual FString		LowLevelDescribe() override;
	virtual void		LowLevelSend( void* Data, int32 Count ) override;
	virtual int32		IsNetReady( bool Saturate ) override;
	virtual void		FlushNet( bool bIgnoreSimulation = false ) override;
	virtual void		HandleClientPlayer( APlayerController* PC, class UNetConnection* NetConnection ) override;
	virtual bool		ClientHasInitializedLevelFor( const UObject* TestObject ) const override;
	// End UNetConnection interface.

	/** @return The DemoRecording driver object */
	FORCEINLINE class UDemoNetDriver* GetDriver() { return (UDemoNetDriver*)Driver; }

	/** @return The DemoRecording driver object */
	FORCEINLINE const class UDemoNetDriver* GetDriver() const { return (UDemoNetDriver*)Driver; }

	TArray< FQueuedDemoPacket > QueuedDemoPackets;
};


