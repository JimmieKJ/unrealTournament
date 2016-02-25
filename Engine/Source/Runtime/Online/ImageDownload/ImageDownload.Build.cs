// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImageDownload : ModuleRules
{
	public ImageDownload(TargetInfo Target)
    {
        Definitions.Add("IMAGEDOWNLOAD_PACKAGE=1");

        PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"HTTP",
				"SlateCore",
				"ImageWrapper",
			}
			);
    }
}
