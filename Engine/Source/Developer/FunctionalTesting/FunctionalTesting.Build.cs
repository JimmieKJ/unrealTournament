// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
                "RenderCore",
                "AssetRegistry",
                "RHI"
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
