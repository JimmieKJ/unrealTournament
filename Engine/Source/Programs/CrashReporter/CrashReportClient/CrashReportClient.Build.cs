// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashReportClient : ModuleRules
{
	public CrashReportClient(TargetInfo Target)
	{
		PublicIncludePaths.AddRange
		(
			new string[] 
			{ 
				"Runtime/Launch/Public",
				"Programs/CrashReporter/CrashReportClient/Private",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{
				"Core",
				"CoreUObject",
				"CrashDebugHelper",
				"HTTP",
				"Json",
				"Projects",
				"XmlParser",
				"Analytics",
				"AnalyticsET",
				"DesktopPlatform",
				"QoSReporter",
			}
			);

		if (Target.Platform != UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] 
				{
					"Slate",
					"SlateCore",
					"StandaloneRenderer",
					"MessageLog",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);
		}

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include
	}
}
