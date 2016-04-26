// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LauncherCheck : ModuleRules
{
	public LauncherCheck(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                 "HTTP"
			}
		);

        if (UEBuildConfiguration.bUseLauncherChecks)
        {
            PublicDependencyModuleNames.Add("DesktopPlatform");
        }
	}
}
