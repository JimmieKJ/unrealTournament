// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MaterialShaderQualitySettings : ModuleRules
{
    public MaterialShaderQualitySettings(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "RHI",
			}
		);
        PrivateIncludePathModuleNames.Add("Engine");

        if (UEBuildConfiguration.bBuildEditor == true)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[] {            
       				"Slate",
				    "SlateCore",
				    "EditorStyle",
                    "PropertyEditor",
                    "TargetPlatform",
                    "InputCore",
                }
            );

		}

	}
}
