// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LinuxTargetPlatform : ModuleRules
{
    public LinuxTargetPlatform(TargetInfo Target)
	{
        BinariesSubFolder = "Linux";

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"TargetPlatform",
				"DesktopPlatform",
				"Projects"
			}
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
				"Developer/LinuxTargetPlatform/Classes"
			}
        );

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}
	}
}
