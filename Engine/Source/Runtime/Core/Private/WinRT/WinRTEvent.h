// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AllowWinRTPlatformTypes.h"


/**
 * Implements the WinRT version of the FEvent interface.
 */
class FEventWinRT
	: public FEvent
{
public:

	/** Default constructor. */
	FEventWinRT()
		: Event(NULL)
	{ }

	/** Virtual destructor. */
	virtual ~FEventWinRT()
	{
		if (Event != NULL)
		{
			CloseHandle(Event);
		}
	}

public:

	// FEvent interface

	virtual bool Create (bool bIsManualReset = false) override
	{
		// Create the event and default it to non-signaled
//		Event = CreateEvent(NULL, bIsManualReset, 0, NULL);
		Event = CreateEventEx(NULL, NULL, bIsManualReset ? CREATE_EVENT_MANUAL_RESET : 0, EVENT_ALL_ACCESS);
		ManualReset = bIsManualReset;
		StatID = FEventStats::CreateStatID();
		return Event != NULL;
	}

	virtual bool IsManualReset() override
	{
		return ManualReset;
	}

	virtual void Trigger () override
	{
		check(Event);

		SetEvent(Event);
	}

	virtual void Reset () override
	{
		check(Event);

		ResetEvent(Event);
	}

	virtual bool Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats = false) override;

private:

	/** Holds the handle to the event. */
	HANDLE Event;

	/** Whether the signaled state of the event needs to be reset manually. */
	bool ManualReset;

	/** Stat ID of this event. */
	TStatId StatID;
};


#include "HideWinRTPlatformTypes.h"
