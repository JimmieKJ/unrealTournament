// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Profiler : ModuleRules
{
	public Profiler( TargetInfo Target )
	{
		PrivateIncludePaths.AddRange
		(
			new string[] {
				"Developer/Profiler/Private",
				"Developer/Profiler/Private/Widgets",
			}
		);

		PublicDependencyModuleNames.AddRange
		(
			new string[] {
				"Core",
                "InputCore",
				"RHI",
				"RenderCore",
				"Slate",
                "EditorStyle",
				"ProfilerClient",
				"DesktopPlatform",
			}
		);

        if (UEBuildConfiguration.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
					"Engine",
				}
            );
        }

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SlateCore",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Messaging",
				"SessionServices",
			}
		);

		PublicIncludePaths.AddRange
		(
			new string[] {
				"Developer/Profiler/Public",
				"Developer/Profiler/Public/Interfaces",
			}
		);
	}
}
