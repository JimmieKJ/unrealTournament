// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HardwareSurvey : ModuleRules
{
	public HardwareSurvey(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Runtime/HardwareSurvey/Private",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/HardwareSurvey/Public",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
		new string[] {
				"Analytics",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Analytics",
			}
		);

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
