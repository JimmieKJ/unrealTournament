// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CharacterAI : ModuleRules
{
    public CharacterAI(TargetInfo Target)
	{
        PrivateIncludePaths.Add("CharacterAI/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"ShaderCore",
				"RenderCore",
				"RHI",
                "AIModule",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Slate",
				"Renderer",
			}
			);

		if (UEBuildConfiguration.bBuildEditor == true)
		{
			//@TODO: Needed for the triangulation code used for sprites (but only in editor mode)
			//@TOOD: Try to move the code dependent on the triangulation code to the editor-only module
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
