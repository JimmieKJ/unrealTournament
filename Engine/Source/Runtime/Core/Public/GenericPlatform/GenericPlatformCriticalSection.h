// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Timespan.h"

/** Platforms that don't need a working FSystemWideCriticalSection can just typedef this one */
class FSystemWideCriticalSectionNotImplemented
{
public:
	/** Construct a named, system-wide critical section and attempt to get access/ownership of it */
	explicit FSystemWideCriticalSectionNotImplemented(const FString& Name, FTimespan Timeout = FTimespan::Zero());

	/** Destructor releases system-wide critical section if it is currently owned */
	~FSystemWideCriticalSectionNotImplemented() {}

	/**
	 * Does the calling thread have ownership of the system-wide critical section?
	 *
	 * @return True if the system-wide lock is obtained.
	 */
	bool IsValid() const { return false; }

	/** Releases system-wide critical section if it is currently owned */
	void Release() {}

private:
	FSystemWideCriticalSectionNotImplemented(const FSystemWideCriticalSectionNotImplemented&);
	FSystemWideCriticalSectionNotImplemented& operator=(const FSystemWideCriticalSectionNotImplemented&);
};
