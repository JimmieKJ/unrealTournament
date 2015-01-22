// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneCore : ModuleRules
{
	public MovieSceneCore(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/MovieSceneCore/Private");

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
