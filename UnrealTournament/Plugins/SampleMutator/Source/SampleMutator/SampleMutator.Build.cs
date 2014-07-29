// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SampleMutator : ModuleRules
	{
		public SampleMutator(TargetInfo Target)
        {
            PrivateIncludePaths.Add("SampleMutator/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "UnrealTournament",
				}
				);
		}
	}
}