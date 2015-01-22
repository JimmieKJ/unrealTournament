// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UserFeedback : ModuleRules
{
    public UserFeedback(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SlateCore",
				"UnrealEd",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
                "EditorStyle",
				"Engine",
				"Analytics",
                "InputCore",
			}
		);
	}
}
