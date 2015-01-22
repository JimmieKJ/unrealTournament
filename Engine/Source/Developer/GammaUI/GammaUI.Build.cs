// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GammaUI : ModuleRules
{
	public GammaUI(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
				"Engine",
				"Slate",
				"SlateCore",
                "InputCore",
                "EditorStyle"
			}
		);
	}
}
