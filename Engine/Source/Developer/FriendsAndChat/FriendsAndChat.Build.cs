// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FriendsAndChat : ModuleRules
{
	public FriendsAndChat(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Slate",
			}
		);

		PrivateDependencyModuleNames.AddRange(
		new string[]
			{
				"SlateCore",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Developer/FriendsAndChat/Private",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Developer/FriendsAndChat/Public",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
		new string[] {
				"EditorStyle",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"EditorStyle",
			}
		);
	}
}
