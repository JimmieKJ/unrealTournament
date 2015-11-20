// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class AnimGraphRuntime : ModuleRules
{
	public AnimGraphRuntime(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/AnimGraphRuntime/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject", 
				"Engine"
			}
		);
	}
}
