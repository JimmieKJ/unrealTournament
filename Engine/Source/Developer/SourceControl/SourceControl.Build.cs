// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SourceControl : ModuleRules
{
	public SourceControl(TargetInfo Target)
	{
        PrivateIncludePaths.Add("Developer/SourceControl/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
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
					"Engine",
					"UnrealEd",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"AssetTools"
				}
			);
	        
			CircularlyReferencedDependentModules.Add("UnrealEd");
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
