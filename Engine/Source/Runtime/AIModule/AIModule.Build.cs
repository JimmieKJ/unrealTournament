// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AIModule : ModuleRules
	{
        public AIModule(TargetInfo Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
                    "Runtime/AIModule/Public",
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/AIModule/Private",
                    "Runtime/Engine/Private",
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",    
               		"GameplayTags"
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RHI",
                    "RenderCore",
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					// ... add any modules that your module loads dynamically here ...
				}
				);

			if (UEBuildConfiguration.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");

                PrivateDependencyModuleNames.Add("AITestSuite");                
                CircularlyReferencedDependentModules.Add("AITestSuite");
			}

            if (UEBuildConfiguration.bCompileRecast)
            {
                PrivateDependencyModuleNames.Add("Navmesh");
                Definitions.Add("WITH_RECAST=1");
            }
            else
            {
                // Because we test WITH_RECAST in public Engine header files, we need to make sure that modules
                // that import us also have this definition set appropriately.  Recast is a private dependency
                // module, so it's definitions won't propagate to modules that import Engine.
                Definitions.Add("WITH_RECAST=0");
            }
		}
	}
}
