// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class SoundMod : ModuleRules
    {
        public SoundMod(TargetInfo Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
				}
            );

			Definitions.AddRange(
				new string[] {
					"BUILDING_STATIC"
				}
			);

			// Link with managed Perforce wrapper assemblies
			AddThirdPartyPrivateStaticDependencies(Target, "coremod");
		
		}
    }
}
