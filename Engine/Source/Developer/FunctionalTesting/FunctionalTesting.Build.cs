// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FunctionalTesting : ModuleRules
{
	public FunctionalTesting(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
                "MessageLog",
                "AIModule",
                "RenderCore"
			}
			);

		PrivateIncludePaths.AddRange(
			new string[] 
			{
				"MessageLog/Public",
				"Stats/Public",
				"Developer/FunctionalTesting/Private",
			}
			);
	}
}
