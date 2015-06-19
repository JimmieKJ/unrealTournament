// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/* Dependencies
 *****************************************************************************/

#if WITH_EDITOR
	#include "UnrealEd.h" // this needs to be the very first include
#endif

#if WITH_ENGINE
	#include "Engine.h"
	#include "Messaging.h"
	#include "SlateBasics.h"

	#include "MallocProfilerEx.h"
#endif

#include "../Public/LaunchEngineLoop.h"


/* Global functions
 *****************************************************************************/

/**
 * Attempts to set GameName based on the command line and check that we have a game name
 */
bool LaunchSetGameName(const TCHAR *CmdLine);
