// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/** 
 * Interface for ticking runnables when there's only one thread available and 
 * multithreading is disabled.
 */
class CORE_API FSingleThreadRunnable
{
public:

	virtual ~FSingleThreadRunnable() { }

	/* Tick function. */
	virtual void Tick() = 0;
};
