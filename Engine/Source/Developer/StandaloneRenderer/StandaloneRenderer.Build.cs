// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StandaloneRenderer : ModuleRules
{
	public StandaloneRenderer(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Developer/StandaloneRenderer/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ImageWrapper",
				"InputCore",
				"SlateCore",
			}
			);

		AddThirdPartyPrivateStaticDependencies(Target, "OpenGL");

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			// @todo: This should be private? Not sure!!
			AddThirdPartyPrivateStaticDependencies(Target, "DX11");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.Add("QuartzCore");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicFrameworks.AddRange(new string[] { "OpenGLES", "GLKit" });
			// weak for IOS8 support since CAMetalLayer is in QuartzCore
			PublicWeakFrameworks.AddRange(new string[] { "QuartzCore" });
		}
	}
}
