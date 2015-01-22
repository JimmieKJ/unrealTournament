// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationWindow : ModuleRules
	{
		public AutomationWindow(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"AutomationController",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"DesktopPlatform",
                    "InputCore",
					"Slate",
					"SlateCore",
                    "EditorStyle",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"SessionServices",
				}
			);
		}
	}
}
