// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SampleGameMode : ModuleRules
	{
		public SampleGameMode(TargetInfo Target)
        {
            PrivateIncludePaths.Add("SampleGameMode/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "UnrealTournament",
					"InputCore",
					"SlateCore",
				}
				);
		}
	}
}