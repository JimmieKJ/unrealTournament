// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealAudio : ModuleRules
{
	public UnrealAudio(TargetInfo Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/UnrealAudio/Private",
				"Runtime/UnrealAudio/Private/Tests",
			}
		);

		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/UnrealAudio/Public",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject", 
			}
		);
	}
}