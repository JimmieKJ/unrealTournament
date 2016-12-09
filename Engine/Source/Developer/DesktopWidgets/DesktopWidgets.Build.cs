// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DesktopWidgets : ModuleRules
{
	public DesktopWidgets(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
				"SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "DesktopPlatform",
                "InputCore",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/DesktopWidgets/Private",
				"Developer/DesktopWidgets/Private/Widgets",
                "Developer/DesktopWidgets/Private/Widgets/Input",
			}
		);
	}
}