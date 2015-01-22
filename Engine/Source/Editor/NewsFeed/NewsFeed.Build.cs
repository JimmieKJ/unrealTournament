// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NewsFeed : ModuleRules
{
	public NewsFeed(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Analytics",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
				"EditorStyle",
				"Engine",
				"HTTP",
				"ImageWrapper",
                "InputCore",
				"Json",
				"OnlineSubsystem",
				"Slate",
				"SlateCore",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/NewsFeed/Private",
				"Editor/NewsFeed/Private/Implementation",
				"Editor/NewsFeed/Private/Models",
				"Editor/NewsFeed/Private/Widgets",
			}
		);
	}
}
