// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
 
public class PluginCreator : ModuleRules
{
	public PluginCreator(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(new string[] { "PluginCreator/Private", "PluginCreator/Private/UI", "PluginCreator/Private/Helpers" });

		PrivateIncludePathModuleNames.AddRange(new string[]
			{ 
				"DesktopPlatform", 
 				"HardwareTargeting",
			});

		PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
 				"InputCore",
				"UnrealEd",
 				"EditorStyle",
 				"Projects",
				"GameProjectGeneration" 
			});

		DynamicallyLoadedModuleNames.AddRange(new string[]
			{
				"DesktopPlatform",
			});
	}
}