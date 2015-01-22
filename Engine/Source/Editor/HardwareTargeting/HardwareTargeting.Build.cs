// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HardwareTargeting : ModuleRules
{
	public HardwareTargeting(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Engine",
				"InputCore",
				"SlateCore",
				"Slate",
				"EditorStyle",
				"UnrealEd",
				"Settings",
				"EngineSettings",
			}
		);
	}
}
