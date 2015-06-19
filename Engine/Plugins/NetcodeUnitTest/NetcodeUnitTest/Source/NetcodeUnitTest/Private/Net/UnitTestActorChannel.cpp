// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NetcodeUnitTestPCH.h"

#include "Net/UnitTestActorChannel.h"


/**
 * Default constructor
 */
UUnitTestActorChannel::UUnitTestActorChannel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UUnitTestActorChannel::ReceivedBunch(FInBunch& Bunch)
{
	// If Actor is NULL, and we are processing a bunch, then we are most likely initializing the actor channel
	if (Actor == NULL)
	{
		GIsInitializingActorChan = true;
	}


	// As a hack to block sending an 'NMT_ActorChannelFailed' event that kicks the client,
	// the control channel is NULL'd when an actor channel is blocked, and this is in charge of recovering from that
	// NOTE: If you update this, update the below function too
	UChannel* ControlChan = Connection->Channels[0];

	Super::ReceivedBunch(Bunch);

	if (ControlChan != NULL && Connection->Channels[0] == NULL)
	{
		Connection->Channels[0] = ControlChan;
	}

	GIsInitializingActorChan = false;
}

void UUnitTestActorChannel::Tick()
{
	// If Actor is NULL and we're about to process queued bunches, we may be about to do a (delayed) actor channel initialization
	if (Actor == NULL && QueuedBunches.Num() > 0)
	{
		GIsInitializingActorChan = true;
		GActiveReceiveUnitConnection = Connection;
	}

	// NOTE: If you update this, update the above function too
	UChannel* ControlChan = Connection->Channels[0];

	Super::Tick();

	if (ControlChan != NULL && Connection->Channels[0] == NULL)
	{
		Connection->Channels[0] = ControlChan;
	}

	GActiveReceiveUnitConnection = NULL;
	GIsInitializingActorChan = false;
}

