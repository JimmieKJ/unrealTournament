// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class BuildPatchToolTarget : TargetRules
{
	public BuildPatchToolTarget( TargetInfo Target )
	{
		Type = TargetType.Program;
		bOutputPubliclyDistributable = true;
	}

	//
	// TargetRules interface.
	//

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames)
	{
		OutBuildBinaryConfigurations.Add(
			new UEBuildBinaryConfiguration(
				InType: UEBuildBinaryType.Executable,
				InModuleNames: new List<string>() {
					"BuildPatchTool"
				}
			)
		);
	}

	public override bool ShouldCompileMonolithic(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
	{
		return true;
	}

	public override void SetupGlobalEnvironment(
		TargetInfo Target,
		ref LinkEnvironmentConfiguration OutLinkEnvironmentConfiguration,
		ref CPPEnvironmentConfiguration OutCPPEnvironmentConfiguration)
	{
		UEBuildConfiguration.bBuildEditor = false;
		UEBuildConfiguration.bCompileAgainstEngine = false;
		UEBuildConfiguration.bCompileAgainstCoreUObject = false;
		UEBuildConfiguration.bCompileLeanAndMeanUE = true;

		OutCPPEnvironmentConfiguration.Definitions.Add("WITH_BUILDPATCHGENERATION=1");

		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = true;
		OutLinkEnvironmentConfiguration.bHasExports = false;
	}
    public override bool GUBP_AlwaysBuildWithTools(UnrealTargetPlatform InHostPlatform, out bool bInternalToolOnly, out bool SeparateNode, out bool CrossCompile)
    {
        bInternalToolOnly = true;
        SeparateNode = true;
		CrossCompile = false;
        return true;
    }
}
