// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISlateReflectorModule.h"
#include "SlateViewer.h"


/**
 * Run the SlateViewer .
 */
void RunSlateViewer(const TCHAR* Commandline);

/**
 * Spawn the contents of the web browser tab
 */
TSharedRef<SDockTab> SpawnWebBrowserTab(const FSpawnTabArgs& Args);
