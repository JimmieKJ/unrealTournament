// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MainFrame.h"


/* Private dependencies
 *****************************************************************************/

#include "AboutScreen.h"
#include "CreditsScreen.h"
#include "AssetToolsModule.h"
#include "CrashTracker.h"
#include "DesktopPlatformModule.h"
#include "ISourceControlModule.h"
#include "ITranslationEditor.h"
#include "GameProjectGenerationModule.h"
#include "GenericApplication.h"
#include "GlobalEditorCommonCommands.h"
#include "MainFrame.h"
#include "MessageLog.h"
#include "MessageLogModule.h"
#include "ModuleManager.h"
#include "MRUFavoritesList.h"
#include "SuperSearchModule.h"
#include "OutputLogModule.h"
//#include "SearchUI.h"
#include "SourceCodeNavigation.h"
#include "SourceControlWindows.h"
#include "ISettingsModule.h"
#include "ILocalizationDashboardModule.h"
#include "TargetPlatform.h"
#include "PlatformInfo.h"
#include "UnrealEd.h"

#include "../../LevelEditor/Public/LevelEditor.h"
#include "../../LevelEditor/Public/SLevelViewport.h"

#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetDebugUtilities.h"

#include "ISourceCodeAccessModule.h"


/* Private includes
 *****************************************************************************/

DECLARE_LOG_CATEGORY_EXTERN(LogMainFrame, Log, All);

const FText StaticGetApplicationTitle( const bool bIncludeGameName );

#include "MainFrameActions.h"

#include "CookContentMenu.h"
#include "PackageProjectMenu.h"
#include "RecentProjectsMenu.h"
#include "SettingsMenu.h"
#include "MainFrameTranslationEditorMenu.h"

#include "MainMenu.h"
#include "RootWindowLocation.h"
#include "MainFrameHandler.h"
#include "MainFrameModule.h"
