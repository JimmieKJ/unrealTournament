// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class SlateViewerTarget : TargetRules
{
	public SlateViewerTarget(TargetInfo Target)
	{
		Type = TargetType.Program;
	}

	//
	// TargetRules interface.
	//

	public override bool GetSupportedPlatforms(ref List<UnrealTargetPlatform> OutPlatforms)
	{
		if (UnrealBuildTool.UnrealBuildTool.GetAllDesktopPlatforms(ref OutPlatforms, false) == true)
		{
			OutPlatforms.Add(UnrealTargetPlatform.IOS);
			return true;
		}

		return false;
	}

	public override void SetupBinaries(
		TargetInfo Target,
		ref List<UEBuildBinaryConfiguration> OutBuildBinaryConfigurations,
		ref List<string> OutExtraModuleNames
		)
	{
		OutBuildBinaryConfigurations.Add(
			new UEBuildBinaryConfiguration(	InType: UEBuildBinaryType.Executable,
											InModuleNames: new List<string>() { "SlateViewer" } )
			);

        OutExtraModuleNames.Add("EditorStyle");
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

		// SlateViewer doesn't ever compile with the engine linked in
		UEBuildConfiguration.bCompileAgainstEngine = false;

		// We need CoreUObject compiled in as the source code access module requires it
		UEBuildConfiguration.bCompileAgainstCoreUObject = true;

		// SlateViewer.exe has no exports, so no need to verify that a .lib and .exp file was emitted by
		// the linker.
		OutLinkEnvironmentConfiguration.bHasExports = false;
	}
}
