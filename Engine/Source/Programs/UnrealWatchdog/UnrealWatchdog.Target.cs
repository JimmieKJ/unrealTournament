// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealWatchdogTarget : TargetRules
{
	public UnrealWatchdogTarget(TargetInfo Target)
	{
		Type = TargetType.Program;
        UndecoratedConfiguration = UnrealTargetConfiguration.Shipping;
    }

    //
    // TargetRules interface.
    //

    public override bool GetSupportedPlatforms(ref List<UnrealTargetPlatform> OutPlatforms)
	{
		OutPlatforms.Add(UnrealTargetPlatform.Win32);
		OutPlatforms.Add(UnrealTargetPlatform.Win64);
		//OutPlatforms.Add(UnrealTargetPlatform.Mac);
		//OutPlatforms.Add(UnrealTargetPlatform.Linux);
		return true;
	}

    public override bool GetSupportedConfigurations(ref List<UnrealTargetConfiguration> OutConfigurations, bool bIncludeTestAndShippingConfigs)
    {
        if (base.GetSupportedConfigurations(ref OutConfigurations, bIncludeTestAndShippingConfigs))
        {
            if (bIncludeTestAndShippingConfigs)
            {
                OutConfigurations.Add(UnrealTargetConfiguration.Shipping);
            }
            OutConfigurations.Add(UnrealTargetConfiguration.Debug);
            return true;
        }
        else
        {
            return false;
        }
    }

    public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutBuildBinaryConfigurations.Add(
			new UEBuildBinaryConfiguration(	InType: UEBuildBinaryType.Executable,
											InModuleNames: new List<string>() { "UnrealWatchdog" } )
			);
	}

	public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
	{
		return true;
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration
		)
	{
		// Lean and mean
		UEBuildConfiguration.bCompileLeanAndMeanUE = true;
		BuildConfiguration.bUseMallocProfiler = false;

        UEBuildConfiguration.bUseLoggingInShipping = true;

        // No editor needed
        UEBuildConfiguration.bBuildEditor = false;
		UEBuildConfiguration.bBuildWithEditorOnlyData = false;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		UEBuildConfiguration.bCompileAgainstEngine = false;
		UEBuildConfiguration.bCompileAgainstCoreUObject = false;
		UEBuildConfiguration.bBuildDeveloperTools = false;

		// Console application, not a Windows app (sets entry point to main(), instead of WinMain())
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = true;
	}
}
