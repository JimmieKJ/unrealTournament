// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "MatineeDelegates.h"

FMatineeDelegates& FMatineeDelegates::Get()
{
	// return the singleton object
	static FMatineeDelegates Singleton;
	return Singleton;
}
