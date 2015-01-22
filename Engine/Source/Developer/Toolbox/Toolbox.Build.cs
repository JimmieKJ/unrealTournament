// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Toolbox : ModuleRules
{
	public Toolbox(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"GammaUI",
				"MainFrame",
				"ModuleUI"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
                "InputCore",
				"Slate",
				"SlateCore",
				"SlateReflector",
                "EditorStyle",
				"DesktopPlatform",
				"AppFramework"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"GammaUI", 
				"MainFrame",
				"ModuleUI"
			}
		);
	}
}