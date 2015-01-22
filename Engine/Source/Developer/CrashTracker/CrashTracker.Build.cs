// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashTracker : ModuleRules
{
	public CrashTracker(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Developer/CrashTracker/Private");

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"ImageWrapper",
				"RenderCore",
				"RHI",
				"ShaderCore",
				"Slate",
				"SlateCore",
			}
		);
	}
}
