// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MessageLog : ModuleRules
{
	public MessageLog(TargetInfo Target)
	{
        PrivateIncludePaths.AddRange(
            new string[] {
				"Developer/MessageLog/Private",
				"Developer/MessageLog/Private/Presentation",
				"Developer/MessageLog/Private/UserInterface",
                "Developer/MessageLog/Private/Model",
			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
                "InputCore",
                "EditorStyle",
			}
		);

		if (UEBuildConfiguration.bBuildEditor)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Documentation",
					"IntroTutorials",
				}
			);		
		}
	}
}
