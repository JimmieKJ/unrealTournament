// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class SoundVisualizations : ModuleRules
    {
        public SoundVisualizations(TargetInfo Target)
        {
            PrivateIncludePaths.Add("SoundVisualizations/Private");

            PublicDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
				}
                );

			AddThirdPartyPrivateStaticDependencies(Target, "Kiss_FFT");
        }
    }
}
