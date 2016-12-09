// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Net/UnitTestChannel.h"
#include "Engine/NetConnection.h"
#include "NetcodeUnitTest.h"




/**
 * Default constructor
 */
UUnitTestChannel::UUnitTestChannel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ReceivedBunchDel()
	, bVerifyOpen(false)
{
	ChType = CHTYPE_MAX;
}

/**
 * @todo #JohnBDoc
 */
void UUnitTestChannel::Init(UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally)
{
	// If the channel type is still default, assume this is a control channel (since that is the only time ChType should be default)
	if (ChType == CHTYPE_MAX)
	{
		if (InChIndex != 0)
		{
			UE_LOG(LogUnitTest, Warning, TEXT("Unit test channel type was CHTYPE_MAX, for non-control channel"));
		}

		ChType = CHTYPE_Control;
	}

	Super::Init(InConnection, InChIndex, InOpenedLocally);
}

/**
 * @todo #JohnBDoc
 */
void UUnitTestChannel::ReceivedBunch(FInBunch& Bunch)
{
	// Pass on to delegate
	ReceivedBunchDel.ExecuteIfBound(Bunch);
}

/**
 * @todo #JohnBDoc
 */
void UUnitTestChannel::Tick()
{
	Super::Tick();

	// Copied from the control channel code
	// @todo #JohnBBug: This can spam the log sometimes, upon unit test error; fix this
	if (!OpenAcked)
	{
		int32 Count = 0;

		for (FOutBunch* Out=OutRec; Out; Out=Out->Next)
		{
			if (!Out->ReceivedAck)
			{
				Count++;
			}
		}

		if (Count > 8)
		{
			return;
		}

		for (FOutBunch* Out=OutRec; Out; Out=Out->Next)
		{
			if (!Out->ReceivedAck)
			{
				float Wait = Connection->Driver->Time - Out->Time;

				if (Wait > 1.f)
				{
					UE_LOG(LogUnitTest, Log, TEXT("UnitTestChannel %i ack timeout); resending %i..."), ChIndex, Out->ChSequence);

					Connection->SendRawBunch(*Out, 0);
				}
			}
		}
	}
}

