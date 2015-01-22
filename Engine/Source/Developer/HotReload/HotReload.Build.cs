// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class HotReload : ModuleRules
{
	public HotReload(TargetInfo Target)
	{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"DirectoryWatcher",
					"DesktopPlatform"
				}
			);

            if (UEBuildConfiguration.bCompileAgainstEngine)
            {
                PrivateDependencyModuleNames.AddRange(
				    new string[] 
				    {
					    "Engine",
					    "UnrealEd", 
				    }
			    );
            }
	}
}