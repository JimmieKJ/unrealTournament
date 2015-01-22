// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Phya : ModuleRules
	{
        public Phya(TargetInfo Target)
		{
            PrivateIncludePaths.Add("Phya/Private");
            PrivateIncludePaths.Add("Phya/Private/PhyaLib/include");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine"
				}
				);
		}
	}
}
