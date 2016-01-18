// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Localization : ModuleRules
{
	public Localization(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
                "UnrealEd",
                "EditorStyle",
				"DesktopPlatform",
                "SourceControl",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Editor/Localization/Private",
			}
		);

        PublicIncludePaths.AddRange(
			new string[]
			{
				"Editor/Localization/Public",
			}
		);
	}
}