// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class CrashReportClientTarget : TargetRules
{
	public CrashReportClientTarget(TargetInfo Target)
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
		OutPlatforms.Add(UnrealTargetPlatform.Mac);
		OutPlatforms.Add(UnrealTargetPlatform.Linux);
		return true;
	}

    public override bool GetSupportedConfigurations(ref List<UnrealTargetConfiguration> OutConfigurations, bool bIncludeTestAndShippingConfigs)
	{
		if( base.GetSupportedConfigurations( ref OutConfigurations, bIncludeTestAndShippingConfigs ) )
		{
			if( bIncludeTestAndShippingConfigs )
			{ 
				OutConfigurations.Add( UnrealTargetConfiguration.Shipping );
			}
			OutConfigurations.Add( UnrealTargetConfiguration.Debug );
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
											InModuleNames: new List<string>() { "CrashReportClient" })
			);

		if (Target.Platform != UnrealTargetPlatform.Linux)
		{
			OutExtraModuleNames.Add("EditorStyle");
		}
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

		// CrashReportClient doesn't ever compile with the engine linked in
		UEBuildConfiguration.bCompileAgainstEngine = false;
		UEBuildConfiguration.bCompileAgainstCoreUObject = true;
		UEBuildConfiguration.bUseLoggingInShipping = true;

		UEBuildConfiguration.bIncludeADO = false;
		
		// Do not include ICU for Linux (this is a temporary workaround, separate headless CrashReportClient target should be created, see UECORE-14 for details).
		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			UEBuildConfiguration.bCompileICU = false;
		}

		// CrashReportClient.exe has no exports, so no need to verify that a .lib and .exp file was emitted by
		// the linker.
		OutLinkEnvironmentConfiguration.bHasExports = false;

		UEBuildConfiguration.bUseChecksInShipping = true;

		// Epic Games Launcher needs to run on OS X 10.9, so CrashReportClient needs this as well
		OutCPPEnvironmentConfiguration.bEnableOSX109Support = true;
	}
    public override bool GUBP_AlwaysBuildWithTools(UnrealTargetPlatform InHostPlatform, out bool bInternalToolOnly, out bool SeparateNode, out bool CrossCompile)
	{
		bInternalToolOnly = false;
		SeparateNode = false;
		CrossCompile = true;
		return true;
	}
	public override List<UnrealTargetPlatform> GUBP_ToolPlatforms(UnrealTargetPlatform InHostPlatform)
	{
		if (InHostPlatform == UnrealTargetPlatform.Win64)
		{
			return new List<UnrealTargetPlatform> { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Win32};
		}
		return base.GUBP_ToolPlatforms(InHostPlatform);
	}

	public override List<UnrealTargetConfiguration> GUBP_ToolConfigs( UnrealTargetPlatform InHostPlatform )
	{
		return new List<UnrealTargetConfiguration> { UnrealTargetConfiguration.Shipping };
	}
}
