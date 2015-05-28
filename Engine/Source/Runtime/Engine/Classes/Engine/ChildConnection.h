// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChildConnection.generated.h"


/**
 * Represents a secondary split screen connection that reroutes calls to the parent connection.
 */
UCLASS(MinimalAPI,transient,config=Engine)
class UChildConnection
	: public UNetConnection
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(transient)
	class UNetConnection* Parent;

public:

	// UNetConnection interface.

	virtual UChildConnection* GetUChildConnection() override
	{
		return this;
	}

	virtual FString LowLevelGetRemoteAddress(bool bAppendPort=false) override
	{
		return Parent->LowLevelGetRemoteAddress(bAppendPort);
	}

	virtual FString LowLevelDescribe() override
	{
		return Parent->LowLevelDescribe();
	}

	virtual void LowLevelSend( void* Data, int32 Count ) override
	{
	}

	virtual void InitSendBuffer() override
	{
		Parent->InitSendBuffer();
	}

	virtual void AssertValid() override
	{
		Parent->AssertValid();
	}

	virtual void SendAck( int32 PacketId, bool FirstTime=1, bool bHavePingAckData=0, uint32 PingAckData=0 ) override
	{
	}

	virtual void FlushNet(bool bIgnoreSimulation = false) override
	{
		Parent->FlushNet(bIgnoreSimulation);
	}

	virtual int32 IsNetReady(bool Saturate) override
	{
		return Parent->IsNetReady(Saturate);
	}

	virtual void Tick() override
	{
		State = Parent->State;
	}

	virtual void HandleClientPlayer(class APlayerController* PC, class UNetConnection* NetConnection) override;
	virtual void CleanUp() override;
};
