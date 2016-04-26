// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnitTestFramework : ModuleRules
{
	public UnitTestFramework(TargetInfo Target)
	{
		PrivateIncludePaths.Add("UnitTestFramework/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
			}
		);

		if (UEBuildConfiguration.bBuildEditor == true)
		{
		}
	}
}
