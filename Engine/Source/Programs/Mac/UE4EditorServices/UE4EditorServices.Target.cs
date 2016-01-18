// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UE4EditorServicesTarget : TargetRules
{
	public UE4EditorServicesTarget(TargetInfo Target)
	{
		Type = TargetType.Program;
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
		OutBuildBinaryConfigurations.Add(
			new UEBuildBinaryConfiguration( InType: UEBuildBinaryType.Executable, InModuleNames: new List<string>() { "UE4EditorServices" })
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
		UEBuildConfiguration.bCompileLeanAndMeanUE = true;
		BuildConfiguration.bUseMallocProfiler = false;

		// No editor needed
		UEBuildConfiguration.bCompileICU = false;
		UEBuildConfiguration.bBuildEditor = false;
		// Editor-only data, however, is needed
		UEBuildConfiguration.bBuildWithEditorOnlyData = false;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		UEBuildConfiguration.bCompileAgainstEngine = false;
		UEBuildConfiguration.bCompileAgainstCoreUObject = false;
	}
    public override bool GUBP_AlwaysBuildWithTools(UnrealTargetPlatform InHostPlatform, out bool bInternalToolOnly, out bool SeparateNode, out bool CrossCompile)
	{
		bInternalToolOnly = false;
		SeparateNode = false;
		CrossCompile = false;
		return true;
	}
    public override List<UnrealTargetPlatform> GUBP_ToolPlatforms(UnrealTargetPlatform InHostPlatform)
    {
		if(InHostPlatform == UnrealTargetPlatform.Mac)
		{
			return new List<UnrealTargetPlatform> { InHostPlatform };
		}
		else
		{
			return new List<UnrealTargetPlatform>();
		}
    }
}
