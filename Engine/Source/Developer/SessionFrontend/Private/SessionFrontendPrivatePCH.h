// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/* Dependencies
 *****************************************************************************/

#include "DesktopPlatformModule.h"
#include "EditorStyle.h"
#include "Json.h"
#include "Messaging.h"
#include "ModuleManager.h"
#include "SlateBasics.h"
#include "TargetDeviceServices.h"
#include "TargetPlatform.h"
#include "PlatformInfo.h"

// SessionServices
#include "ISessionInfo.h"
#include "ISessionInstanceInfo.h"
#include "ISessionManager.h"
#include "SessionLogMessage.h"

// @todo gmp: remove these dependencies by making the session front-end extensible
#include "AutomationWindow.h"
#include "ScreenShotComparison.h"
#include "ScreenShotComparisonTools.h"
#include "Profiler.h"


/* Private includes
 *****************************************************************************/

// session browser
#include "SessionBrowserOwnerFilter.h"

#include "SSessionBrowserInstanceListRow.h"
#include "SSessionBrowserInstanceList.h"
#include "SSessionBrowserSessionListRow.h"
#include "SSessionBrowserCommandBar.h"
#include "SSessionBrowser.h"

// session console
#include "SessionConsoleCategoryFilter.h"
#include "SessionConsoleCommands.h"
#include "SessionConsoleLogMessage.h"
#include "SessionConsoleVerbosityFilter.h"

#include "SSessionConsoleLogTableRow.h"
#include "SSessionConsoleCommandBar.h"
#include "SSessionConsoleFilterBar.h"
#include "SSessionConsoleShortcutWindow.h"
#include "SSessionConsoleToolbar.h"
#include "SSessionConsole.h"

// session front-end
#include "SSessionFrontend.h"
