// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealTournamentServer : ModuleRules
{
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
			if (UEBuildConfiguration.bCompileMcpOSS == true)
			{
				PrivateDependencyModuleNames.Add("OnlineSubsystemMcp");
			}
		}
	}
}
