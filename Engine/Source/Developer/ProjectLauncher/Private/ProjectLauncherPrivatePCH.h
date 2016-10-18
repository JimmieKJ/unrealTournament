// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProjectLauncher.h"


/* Private dependencies
 *****************************************************************************/

#include "DesktopPlatformModule.h"
#include "IFilter.h"
#include "ModuleManager.h"
#include "TargetDeviceServices.h"
#include "TargetPlatform.h"
#include "PlatformInfo.h"
#include "TextFilter.h"


/* Static helpers
 *****************************************************************************/

// @todo gmp: move this into a shared library or create a SImageButton widget.
static TSharedRef<SButton> MakeImageButton( const FSlateBrush* ButtonImage, const FText& ButtonTip, const FOnClicked& OnClickedDelegate )
{
	return SNew(SButton)
		.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
		.OnClicked(OnClickedDelegate)
		.ToolTipText(ButtonTip)
		.ContentPadding(2)
		.VAlign(VAlign_Center)
		.ForegroundColor(FSlateColor::UseForeground())
		.Content()
		[
			SNew(SImage)
				.Image(ButtonImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
		];
}


/* Private includes
 *****************************************************************************/

#include "ProjectLauncherCommands.h"
#include "ProjectLauncherModel.h"

#include "SProjectLauncherValidation.h"

#include "SProjectLauncherDelegates.h"
#include "SProjectLauncherBuildConfigurationSelector.h"
#include "SProjectLauncherCookModeSelector.h"
#include "SProjectLauncherFormLabel.h"
#include "SProjectLauncherInstanceTypeSelector.h"
#include "SProjectLauncherProfileLaunchButton.h"
#include "SProjectLauncherProfileNameDescEditor.h"
#include "SProjectLauncherVariantSelector.h"

#include "SProjectLauncherProjectPicker.h"
#include "SProjectLauncherProjectPage.h"

#include "SProjectLauncherBuildPage.h"

#include "SProjectLauncherMapListRow.h"
#include "SProjectLauncherCultureListRow.h"
#include "SProjectLauncherPlatformListRow.h"
#include "SProjectLauncherCookedPlatforms.h"
#include "SProjectLauncherCookByTheBookSettings.h"
#include "SProjectLauncherCookOnTheFlySettings.h"
#include "SProjectLauncherCookPage.h"
#include "SProjectLauncherSimpleCookPage.h"

#include "SProjectLauncherDeviceGroupSelector.h"
#include "SProjectLauncherSimpleDeviceListRow.h"
#include "SProjectLauncherSimpleDeviceListView.h"
#include "SProjectLauncherProfileListRow.h"
#include "SProjectLauncherProfileListView.h"
#include "SProjectLauncherDeployFileServerSettings.h"
#include "SProjectLauncherDeployToDeviceSettings.h"
#include "SProjectLauncherDeployRepositorySettings.h"
#include "SProjectLauncherDeployPage.h"
#include "SProjectLauncherDeployTargets.h"
#include "SProjectLauncherDeployTargetListRow.h"

#include "SProjectLauncherLaunchRoleEditor.h"
#include "SProjectLauncherLaunchCustomRoles.h"
#include "SProjectLauncherLaunchPage.h"

#include "SProjectLauncherPackagingSettings.h"
#include "SProjectLauncherPackagePage.h"

#include "SProjectLauncherArchivePage.h"

#include "SProjectLauncherPreviewPage.h"

#include "SProjectLauncherTaskListRow.h"
#include "SProjectLauncherMessageListRow.h"
#include "SProjectLauncherProgress.h"

#include "SProjectLauncherSettings.h"
#include "SProjectLauncherDeployTaskSettings.h"
#include "SProjectLauncherBuildTaskSettings.h"
#include "SProjectLauncherLaunchTaskSettings.h"

#include "SProjectLauncher.h"
