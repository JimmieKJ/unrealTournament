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

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				// VS2015 updated some of the CRT definitions but not all of the Windows SDK has been updated to match.
				// Microsoft provides this shim library to enable building with VS2015 until they fix everything up.
				//@todo: remove when no longer neeeded (no other code changes should be necessary).
				if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015)
				{
					PublicAdditionalLibraries.Add("legacy_stdio_definitions.lib");
				}
			}

			// Link with managed Perforce wrapper assemblies
			AddThirdPartyPrivateStaticDependencies(Target, "coremod");
		
		}
    }
}
