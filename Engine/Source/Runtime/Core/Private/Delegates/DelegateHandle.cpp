// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"

namespace UE4Delegates_Private
{
	uint64 GNextID = 1;
}

uint64 FDelegateHandle::GenerateNewID()
{
	// Just increment a counter to generate an ID.
	// On the next-to-impossible event that we wrap round to 0, reset back to 1, because we reserve 0 for null delegates.
	uint64 Result = UE4Delegates_Private::GNextID++;
	if (UE4Delegates_Private::GNextID == 0)
	{
		UE4Delegates_Private::GNextID = 1;
	}

	return Result;
}
