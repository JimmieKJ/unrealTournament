// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VREditor : ModuleRules
	{
        public VREditor(TargetInfo Target)
		{
			PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "AppFramework",
				    "Core",
				    "CoreUObject",
				    "Engine",
                    "InputCore",
				    "Slate",
					"SlateCore",
                    "EditorStyle",
				    "UnrealEd",
					"UMG",
					"LevelEditor",
					"HeadMountedDisplay",
                    "ViewportInteraction",
					"Analytics"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"PlacementMode",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
				}
			);
		}
	}
}