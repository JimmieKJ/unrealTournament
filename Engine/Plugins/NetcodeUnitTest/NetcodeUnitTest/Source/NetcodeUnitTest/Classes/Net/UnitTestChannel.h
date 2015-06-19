// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnitTestChannel.generated.h"

/**
 * A net channel for overriding the implementation of traditional net channels,
 * for e.g. blocking control channel messages, to enable minimal clients
 */
UCLASS(transient)
class UUnitTestChannel : public UChannel
{
	GENERATED_UCLASS_BODY()	

	virtual void Init(UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally) override;

	virtual void ReceivedBunch(FInBunch& Bunch) override;

	virtual void Tick() override;


	/**
	 * Delegate for hooking 'ReceivedBunch'
	 *
	 * @param Bunch		The received bunch
	 */
	DECLARE_DELEGATE_OneParam(FReceivedBunchDel, FInBunch&/* Bunch*/);

public:
	/** Delegate for passed on received-bunch calls */
	FReceivedBunchDel	ReceivedBunchDel;

	/** Whether or not this channel should verify it has been opened (resends initial packets until acked, like control channel) */
	bool bVerifyOpen;
};




