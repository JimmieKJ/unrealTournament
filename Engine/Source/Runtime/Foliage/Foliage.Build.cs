// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Foliage: ModuleRules
{
	public Foliage(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
				"RenderCore",
				"RHI"
			}
		);

        PrivateIncludePaths.AddRange(
            new string[] {
                "Runtime/Foliage/Private"
            })
		;
	}
}
