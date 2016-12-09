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
}
