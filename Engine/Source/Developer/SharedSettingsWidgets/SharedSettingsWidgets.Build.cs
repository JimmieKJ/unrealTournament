// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SharedSettingsWidgets : ModuleRules
{
	public SharedSettingsWidgets(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"SourceControl",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ExternalImagePicker",
			}
		);
	}
}
