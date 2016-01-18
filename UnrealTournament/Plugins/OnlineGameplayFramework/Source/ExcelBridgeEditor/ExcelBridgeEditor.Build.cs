// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ExcelBridgeEditor : ModuleRules
{
	public ExcelBridgeEditor(TargetInfo Target)
	{
		Definitions.Add("TEMPLATE_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"OnlineGameplayFramework/Source/ExcelBridgeEditor/Private",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
				"UnrealEd",
                "PropertyEditor",
				"SlateCore",
				"Slate",
				"InputCore",
				"DesktopPlatform",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
		}
	}
}