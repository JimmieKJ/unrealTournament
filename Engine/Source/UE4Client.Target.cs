// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE4ClientTarget : TargetRules
{

    public UE4ClientTarget(TargetInfo Target)
	{
		Type = TargetType.Client;
        bOutputToEngineBinaries = true;
	}

	//
	// TargetRules interface.
	//
	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		base.SetupBinaries(Target, ref OutBuildBinaryConfigurations, ref OutExtraModuleNames);
		OutExtraModuleNames.Add("UE4Game");
	}

	public override void SetupGlobalEnvironment(
        TargetInfo Target,
        ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
        ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
        )
    {
    }

	public override bool ShouldUseSharedBuildEnvironment(TargetInfo Target)
	{
		return true;
	}

	public override void GetModulesToPrecompile(TargetInfo Target, List<string> ModuleNames)
	{
		// Add all the precompiled modules for this target
		ModuleNames.Add("Launch");
		ModuleNames.Add("InputDevice");
		ModuleNames.Add("GameMenuBuilder");
		ModuleNames.Add("GameplayAbilities");
		ModuleNames.Add("XmlParser");
		ModuleNames.Add("UE4Game");
		ModuleNames.Add("AITestSuite");
		ModuleNames.Add("RuntimeAssetCache");
		ModuleNames.Add("UnrealCodeAnalyzerTests");
		if (Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Win64)
		{
			ModuleNames.Add("OnlineSubsystemNull");
			ModuleNames.Add("OnlineSubsystemAmazon");
			if (UEBuildConfiguration.bCompileSteamOSS == true)
			{
				ModuleNames.Add("OnlineSubsystemSteam");
			}
			ModuleNames.Add("OnlineSubsystemFacebook");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux)
		{
			ModuleNames.Add("OnlineSubsystemNull");
			if (UEBuildConfiguration.bCompileSteamOSS == true)
			{
				ModuleNames.Add("OnlineSubsystemSteam");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			ModuleNames.Add("OnlineSubsystemFacebook");
			ModuleNames.Add("OnlineSubsystemIOS");
			ModuleNames.Add("IOSAdvertising");
			ModuleNames.Add("MetalRHI");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// @todo android: Add Android online subsystem
			ModuleNames.Add("AndroidAdvertising");
		}
		else if (Target.Platform == UnrealTargetPlatform.HTML5)
		{
			ModuleNames.Add("OnlineSubsystemNull");
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
				Platforms = new List<UnrealTargetPlatform> { HostPlatform, UnrealTargetPlatform.Win32, UnrealTargetPlatform.Linux /*, UnrealTargetPlatform.IOS, UnrealTargetPlatform.XboxOne, UnrealTargetPlatform.PS4 */};
				break;

			default:
				Platforms = new List<UnrealTargetPlatform>();
				break;
		}

		return Platforms;
    }

    public override List<UnrealTargetConfiguration> GUBP_GetConfigs_MonolithicOnly(UnrealTargetPlatform HostPlatform, UnrealTargetPlatform Platform)
    {
        return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Development };
    }
}
