// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EpicSurvey : ModuleRules
	{
        public EpicSurvey(TargetInfo Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"Core",
					"CoreUObject",
					"Engine",
                    "InputCore",
					"ImageWrapper",
					"LevelEditor",
					"OnlineSubSystem",
					"Slate",
                    "EditorStyle",
				}
			);	// @todo Mac: for some reason CoreUObject and Engine are needed to link in debug on Mac

            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"HTTP",
					"Json",
					"OnlineSubSystem",
					"SlateCore",
					"UnrealEd",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"MainFrame",
				}
			);

            DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					"MainFrame"
				}
			);

			if (UEBuildConfiguration.bCompileMcpOSS == true)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[]
					{
						"OnlineSubsystemMcp",
					}
				);
			}
		}
	}
}