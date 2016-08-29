// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealTournamentServer : ModuleRules
{
    bool IsLicenseeBuild()
    {
        return !Directory.Exists("Runtime/NotForLicensees");
    }

	public UnrealTournamentServer(TargetInfo Target)
	{
		PrivateIncludePaths.Add("../../OrionGame/Source/UnrealTournamentServer/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealTournamentGame",
                "Json",
				"PerfCounters",

			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"OnlineSubsystem",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Analytics",
				"AnalyticsET",
				"HTTP",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics",
				"HTTP",
			}
		);
        
       	if (UEBuildConfiguration.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		if ((Target.Platform == UnrealTargetPlatform.Win32) || 
			(Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Mac) || 
			(Target.Platform == UnrealTargetPlatform.Linux))
		{
			if (!IsLicenseeBuild())
			{
				PrivateDependencyModuleNames.Add("OnlineSubsystemMcp");
			}
		}
	}
}
