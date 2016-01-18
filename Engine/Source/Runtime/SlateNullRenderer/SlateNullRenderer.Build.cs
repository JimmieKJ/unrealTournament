// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateNullRenderer : ModuleRules
{
	public SlateNullRenderer(TargetInfo Target)
	{
        PrivateIncludePaths.Add("Runtime/SlateNullRenderer/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"SlateCore"
			}
		);

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"RenderCore",
				"RHI"
			}
		);
		}
	}
}
