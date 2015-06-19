// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MinidumpDiagnosticsTarget : TargetRules
{
	public MinidumpDiagnosticsTarget( TargetInfo Target )
	{
		Type = TargetType.Program;
	}

	// TargetRules interface.
	public override bool GetSupportedPlatforms( ref List<UnrealTargetPlatform> OutPlatforms )
	{
		OutPlatforms.Add( UnrealTargetPlatform.Win64 );
		OutPlatforms.Add( UnrealTargetPlatform.Mac );
		return true;
	}

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutBuildBinaryConfigurations.Add(
			new UEBuildBinaryConfiguration(	InType: UEBuildBinaryType.Executable,
											InModuleNames: new List<string>() { "MinidumpDiagnostics" })
			);

		UEBuildConfiguration.bCompileICU = false;
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

		// Don't need editor
		UEBuildConfiguration.bBuildEditor = false;

		// MinidumpDiagnostics doesn't ever compile with the engine linked in
		UEBuildConfiguration.bCompileAgainstEngine = false;

		UEBuildConfiguration.bIncludeADO = false;
		//UEBuildConfiguration.bCompileICU = false;

		// MinidumpDiagnostics.exe has no exports, so no need to verify that a .lib and .exp file was emitted by the linker.
		OutLinkEnvironmentConfiguration.bHasExports = false;

		// Do NOT produce additional console app exe
		OutLinkEnvironmentConfiguration.bIsBuildingConsoleApplication = true;

		OutCPPEnvironmentConfiguration.Definitions.Add("MINIDUMPDIAGNOSTICS=1");
	}
    public override bool GUBP_AlwaysBuildWithTools(UnrealTargetPlatform InHostPlatform, out bool bInternalToolOnly, out bool SeparateNode, out bool CrossCompile)
    {
        bInternalToolOnly = true;
        SeparateNode = false;
		CrossCompile = false;
        return true;
    }
}
