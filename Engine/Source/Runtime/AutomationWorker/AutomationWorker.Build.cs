// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationWorker : ModuleRules
	{
		public AutomationWorker(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			); 
			
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AutomationMessages",
					"CoreUObject",
                    "Analytics",
    				"AnalyticsET",
					"Json",
					"JsonUtilities"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Messaging",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/AutomationWorker/Private",
				}
			);

			if (UEBuildConfiguration.bCompileAgainstEngine)
			{
				PrivateDependencyModuleNames.Add("Engine");
				PrivateDependencyModuleNames.Add("RHI");
			}
		}
	}
}
