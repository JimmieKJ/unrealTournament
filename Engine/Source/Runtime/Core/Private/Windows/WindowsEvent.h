// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWindowsPlatformTypes.h"


/**
 * Implements the Windows version of the FEvent interface.
 */
class FEventWin
	: public FEvent
{
public:

	/** Default constructor. */
	FEventWin()
		: Event(nullptr)
	{ }

	/** Virtual destructor. */
	virtual ~FEventWin()
	{
		if (Event != nullptr)
		{
			CloseHandle(Event);
		}
	}

	// FEvent interface

	virtual bool Create( bool bIsManualReset = false ) override
	{
		// Create the event and default it to non-signaled
		Event = CreateEvent(nullptr, bIsManualReset, 0, nullptr);
		ManualReset = bIsManualReset;
		StatID = FEventStats::CreateStatID();
		return Event != nullptr;
	}

	virtual bool IsManualReset() override
	{
		return ManualReset;
	}

	virtual void Trigger() override
	{
		check(Event);

		SetEvent(Event);
	}

	virtual void Reset() override
	{
		check(Event);

		ResetEvent(Event);
	}

	virtual bool Wait( uint32 WaitTime, const bool bIgnoreThreadIdleStats = false ) override;

private:

	/** Holds the handle to the event. */
	HANDLE Event;

	/** Whether the signaled state of the event needs to be reset manually. */
	bool ManualReset;

	/** Stat ID of this event. */
	TStatId StatID;
};


#include "HideWindowsPlatformTypes.h"
