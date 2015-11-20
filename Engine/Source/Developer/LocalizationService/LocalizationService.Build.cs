// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LocalizationService : ModuleRules
{
	public LocalizationService(TargetInfo Target)
	{
        PrivateIncludePaths.Add("Developer/LocalizationService/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
				"InputCore",
			}
		);

        if (Target.Platform != UnrealTargetPlatform.Linux)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Slate",
                    "SlateCore",
                    "EditorStyle"
                }
            );
        }

        if (UEBuildConfiguration.bBuildEditor)
        {
			PrivateDependencyModuleNames.AddRange(
                new string[] {
					"UnrealEd",
                    "PropertyEditor",
            }
            );
        }

		if (UEBuildConfiguration.bBuildDeveloperTools)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MessageLog",
				}
			);
		}
	}
}
