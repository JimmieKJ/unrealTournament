// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
		//if (Target.Platform == UnrealTargetPlatform.HTML5)
		//{
		//	OutExtraModuleNames.Add("OnlineSubsystemNull");
		//}
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
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

	public override bool ShouldUseSharedBuildEnvironment(TargetInfo Target)
	{
		return true;
	}

	public override List<UnrealTargetPlatform> GUBP_GetPlatforms_MonolithicOnly(UnrealTargetPlatform HostPlatform)
    {
		List<UnrealTargetPlatform> Platforms = null;

		switch(HostPlatform)
		{
			case UnrealTargetPlatform.Mac:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.IOS, UnrealTargetPlatform.TVOS };
				break;

			case UnrealTargetPlatform.Linux:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform };
				break;

			case UnrealTargetPlatform.Win64:
				Platforms = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Win32, UnrealTargetPlatform.IOS, UnrealTargetPlatform.TVOS, UnrealTargetPlatform.XboxOne, UnrealTargetPlatform.PS4, UnrealTargetPlatform.Android, UnrealTargetPlatform.Linux, UnrealTargetPlatform.HTML5 };
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
    public override List<UnrealTargetConfiguration> GUBP_GetConfigsForPrecompiledBuilds_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
    {
		List<UnrealTargetConfiguration> Platforms = new List<UnrealTargetConfiguration>();
		if(HostPlatform == UnrealTargetPlatform.Mac)
		{
			if(Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.IOS || Platform == UnrealTargetPlatform.TVOS)
			{
				Platforms.Add(UnrealTargetConfiguration.Development);
				Platforms.Add(UnrealTargetConfiguration.Shipping);
			}
		}
		else if(HostPlatform == UnrealTargetPlatform.Win64)
		{
			if(Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Android || Platform == UnrealTargetPlatform.HTML5)
			{
				Platforms.Add(UnrealTargetConfiguration.Development);
				Platforms.Add(UnrealTargetConfiguration.Shipping);
			}
		}
		return Platforms;
    }
	public override bool GUBP_BuildWindowsXPMonolithics()
	{
		return true;
	}
}
