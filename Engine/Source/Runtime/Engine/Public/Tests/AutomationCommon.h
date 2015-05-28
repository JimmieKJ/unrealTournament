// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HardwareInfo.h"
#include "AutomationTest.h"

///////////////////////////////////////////////////////////////////////
// Common Latent commands which are used across test type. I.e. Engine, Network, etc...

DEFINE_LOG_CATEGORY_STATIC(LogEditorAutomationTests, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogEngineAutomationTests, Log, All);
DEFINE_LOG_CATEGORY_STATIC(LogAnalytics, Log, All);

/** Common automation functions */
namespace AutomationCommon
{
	/** Get a string contains the render mode we are currently in */
	static FString GetRenderDetailsString()
	{
		FString HardwareDetailsString;

		// Create the folder name based on the hardware specs we have been provided
		FString HardwareDetails = FHardwareInfo::GetHardwareDetailsString();

		FString RHIString;
		FString RHILookup = NAME_RHI.ToString() + TEXT( "=" );
		if( FParse::Value( *HardwareDetails, *RHILookup, RHIString ) )
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT( "_" ) ) + RHIString;
		}

		FString TextureFormatString;
		FString TextureFormatLookup = NAME_TextureFormat.ToString() + TEXT( "=" );
		if( FParse::Value( *HardwareDetails, *TextureFormatLookup, TextureFormatString ) )
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT( "_" ) ) + TextureFormatString;
		}

		FString DeviceTypeString;
		FString DeviceTypeLookup = NAME_DeviceType.ToString() + TEXT( "=" );
		if( FParse::Value( *HardwareDetails, *DeviceTypeLookup, DeviceTypeString ) )
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT( "_" ) ) + DeviceTypeString;
		}

		FString FeatureLevelString;
		GetFeatureLevelName(GMaxRHIFeatureLevel,FeatureLevelString);
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT( "_" ) ) + FeatureLevelString;
		}

		if(GEngine->StereoRenderingDevice.IsValid())
		{
			HardwareDetailsString = ( HardwareDetailsString + TEXT( "_" ) ) + TEXT("STEREO");
		}

		if( HardwareDetailsString.Len() > 0 )
		{
			//Get rid of the leading "_"
			HardwareDetailsString = HardwareDetailsString.RightChop(1);
		}

		return HardwareDetailsString;
	}

	/** Gets a path used for automation testing (PNG sent to the AutomationTest folder) */
	static void GetScreenshotPath(const FString& TestName, FString& OutScreenshotName, const bool bIncludeHardwareDetails)
	{
		FString PathName = FPaths::AutomationDir() + TestName / FPlatformProperties::PlatformName();

		if( bIncludeHardwareDetails )
		{
			PathName = PathName + TEXT("_") + GetRenderDetailsString();
		}

		FPaths::MakePathRelativeTo(PathName, *FPaths::RootDir());

		OutScreenshotName = FString::Printf(TEXT("%s/%d.png"), *PathName, GEngineVersion.GetChangelist());
	}
}

/**
 * Parameters to the Latent Automation command FTakeEditorScreenshotCommand
 */
struct WindowScreenshotParameters
{
	FString ScreenshotName;
	TSharedPtr<SWindow> CurrentWindow;
};

/**
 * Wait for the given amount of time
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FWaitLatentCommand, float, Duration);

/**
 * Take a screenshot of the active window
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTakeActiveEditorScreenshotCommand, FString, ScreenshotName);

/**
 * Take a screenshot of the active window
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FTakeEditorScreenshotCommand, WindowScreenshotParameters, ScreenshotParameters);

/**
* Latent command to load a map in game
*/
DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FLoadGameMapCommand, FString, MapName);

/**
 * Latent command that requests exit
 */
DEFINE_ENGINE_LATENT_AUTOMATION_COMMAND( FRequestExitCommand );


