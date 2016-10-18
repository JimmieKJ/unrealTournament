// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutomationWindow.h"


/* Private dependencies
 *****************************************************************************/

#include "Messaging.h"

#include "AutomationController.h"
#include "DesktopPlatformModule.h"
#include "IFilter.h"
#include "ISessionInfo.h"
#include "ISessionInstanceInfo.h"
#include "ISessionManager.h"
#include "SlateBasics.h"
#include "EditorStyle.h"


/* Private includes
 *****************************************************************************/

#include "SAutomationWindowCommandBar.h"
#include "SAutomationGraphicalResultBox.h"
#include "SAutomationTestTreeView.h"
#include "SAutomationExportMenu.h"
#include "AutomationFilter.h"
#include "AutomationPresetManager.h"
#include "SAutomationWindow.h"
#include "SAutomationTestItemContextMenu.h"
#include "SAutomationTestItem.h"

#if WITH_EDITOR
#include "UnrealEd.h"
#include "AssetRegistryModule.h"
#endif