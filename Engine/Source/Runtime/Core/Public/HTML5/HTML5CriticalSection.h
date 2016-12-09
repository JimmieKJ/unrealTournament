// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformCriticalSection.h"

/**
 * @todo html5 threads: Dummy critical section
 */
class FHTML5CriticalSection
{
public:

	FHTML5CriticalSection() {}

	/**
	 * Locks the critical section
	 */
	FORCEINLINE void Lock(void)
	{
	}

	/**
	 * Releases the lock on the critical seciton
	 */
	FORCEINLINE void Unlock(void)
	{
	}

private:
	FHTML5CriticalSection(const FHTML5CriticalSection&);
	FHTML5CriticalSection& operator=(const FHTML5CriticalSection&);
};

typedef FHTML5CriticalSection FCriticalSection;
typedef FSystemWideCriticalSectionNotImplemented FSystemWideCriticalSection;
