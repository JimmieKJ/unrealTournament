// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KismetCompiler : ModuleRules
{
	public KismetCompiler(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"UnrealEd",
				"MovieScene",
				"MovieSceneTools",
				"FunctionalTesting",
				"BlueprintGraph",
				"AnimGraph"
			}
			);
	}
}