// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DesktopPlatform : ModuleRules
{
	public DesktopPlatform(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Developer/DesktopPlatform/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Json",
			}
		);

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
            AddThirdPartyPrivateStaticDependencies(Target, "LinuxNativeDialogs");
            AddThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}
	}
}
