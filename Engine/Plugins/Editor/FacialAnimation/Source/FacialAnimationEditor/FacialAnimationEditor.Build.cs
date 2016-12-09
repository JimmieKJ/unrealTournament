// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class FacialAnimationEditor : ModuleRules
	{
		public FacialAnimationEditor(TargetInfo Target)
		{			
			PrivateDependencyModuleNames.AddRange(
				new string[] {
                    "Core",
                    "CoreUObject",
                    "InputCore",
                    "Engine",
                    "FacialAnimation",
                    "SlateCore",
                    "Slate",
					"EditorStyle",
					"UnrealEd",
                    "AudioEditor",
                    "WorkspaceMenuStructure",
                    "DesktopPlatform",
                    "PropertyEditor",
                    "TargetPlatform",
                    "Persona",
                }
			);
		}
	}
}