// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InputBindingEditor : ModuleRules
{
	public InputBindingEditor(TargetInfo Target)
	{

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EditorStyle",
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"InputCore",
				"Engine",
				"UnrealEd",
				"PropertyEditor",
				"Settings"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"SettingsEditor",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Editor/InputBindingEditor/Private",
				"Editor/InputBindingEditor/Private/Widgets",
			}
		);
	}
}
