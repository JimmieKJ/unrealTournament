// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
				"Runtime/FriendsAndChat/Private",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/FriendsAndChat/Public",
			}
		);
	}
}
