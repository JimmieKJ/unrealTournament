// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE4GameTarget : TargetRules
{
	public UE4GameTarget( TargetInfo Target )
	{
		Type = TargetType.Game;
		// Output to Engine/Binaries/<PLATFORM> even if built as monolithic
		bOutputToEngineBinaries = true;
	}

	public override bool GetSupportedPlatforms(ref List<UnrealTargetPlatform> OutPlatforms)
	{
		return UnrealBuildTool.UnrealBuildTool.GetAllPlatforms(ref OutPlatforms, false);
	}

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutExtraModuleNames.Add("UE4Game");
		// this is important - for some reason achievements etc intertwined with the onlinesubsystem and they saved without using a fake OSS. :/
		if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			OutExtraModuleNames.Add("OnlineSubsystemNull");
		}


		if (UnrealBuildTool.UnrealBuildTool.BuildingRocket())
		{
			OutExtraModuleNames.Add("GameMenuBuilder");
			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				OutExtraModuleNames.Add("OnlineSubsystemNull");
				OutExtraModuleNames.Add("OnlineSubsystemAmazon");
				if (UEBuildConfiguration.bCompileSteamOSS == true)
				{
					OutExtraModuleNames.Add("OnlineSubsystemSteam");
				}
				OutExtraModuleNames.Add("OnlineSubsystemFacebook");
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
			{
				OutExtraModuleNames.Add("OnlineSubsystemNull");
				if (UEBuildConfiguration.bCompileSteamOSS == true)
				{
					OutExtraModuleNames.Add("OnlineSubsystemSteam");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				OutExtraModuleNames.Add("OnlineSubsystemFacebook");
				OutExtraModuleNames.Add("OnlineSubsystemIOS");
				OutExtraModuleNames.Add("IOSAdvertising");
			}
			else if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// @todo android: Add Android online subsystem
				OutExtraModuleNames.Add("AndroidAdvertising");
			}
			else if (Target.Platform == UnrealTargetPlatform.HTML5)
			{
				OutExtraModuleNames.Add("OnlineSubsystemNull");
			}
		}
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		if( UnrealBuildTool.UnrealBuildTool.BuildingRocket() )
		{
			UEBuildConfiguration.bCompileLeanAndMeanUE = true;

			// Don't need editor or editor only data
			UEBuildConfiguration.bBuildEditor = false;
			UEBuildConfiguration.bBuildWithEditorOnlyData = false;

			UEBuildConfiguration.bCompileAgainstEngine = true;

			// no exports, so no need to verify that a .lib and .exp file was emitted by the linker.
			OutLinkEnvironmentConfiguration.bHasExports = false;
		}
		else
		{
			// Tag it as a UE4Game build
			OutCPPEnvironmentConfiguration.Definitions.Add("UE4GAME=1");
		}
        if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			// to make World Explorers as small as possible we excluded some items from the engine.
			// uncomment below to make a smaller iOS build
			/*UEBuildConfiguration.bCompileRecast = false;
			UEBuildConfiguration.bCompileSpeedTree = false;
			UEBuildConfiguration.bCompileAPEX = false;
			UEBuildConfiguration.bCompileLeanAndMeanUE = true;
			UEBuildConfiguration.bCompilePhysXVehicle = false;
			UEBuildConfiguration.bCompileFreeType = false;
			UEBuildConfiguration.bCompileForSize = true;*/
		}
	}
	public override List<UnrealTargetPlatform> GUBP_GetPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
		List<UnrealTargetPlatform> Platforms = null;

		switch(HostPlatform)
		{
			case UnrealTargetPlatform.Mac:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS };
				break;

			case UnrealTargetPlatform.Linux:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform };
				break;

			case UnrealTargetPlatform.Win64:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Win32, UnrealTargetPlatform.IOS, UnrealTargetPlatform.XboxOne, UnrealTargetPlatform.PS4, UnrealTargetPlatform.Android, UnrealTargetPlatform.Linux, UnrealTargetPlatform.HTML5 };
				break;

			default:
				Platforms = new List<UnrealTargetPlatform>();
				break;
		}

		return Platforms;
    }
    public override List<UnrealTargetConfiguration> GUBP_GetConfigs_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
    {
        return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Development, UnrealTargetConfiguration.Shipping, UnrealTargetConfiguration.Test };
    }
}
