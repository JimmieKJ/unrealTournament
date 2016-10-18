// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class GameplayDebugger : ModuleRules
    {
        public GameplayDebugger(TargetInfo Target)
        {
            // change this flag to if you need to use deprecated GameplayDebugger module
            // however, it will be removed in next engine version!
            const bool bEnableDeprecatedDebugger = false;

            PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RenderCore",
					"InputCore",
					"SlateCore",
					"Slate",
				});

			PrivateIncludePaths.Add("Developer/GameplayDebugger/Private");

            if (UEBuildConfiguration.bBuildEditor)
			{
                PrivateDependencyModuleNames.AddRange(
                    new string[] {
                        "EditorStyle",
                        "UnrealEd",
                        "PropertyEditor",
                    });
			}

            Definitions.Add("ENABLE_OLD_GAMEPLAY_DEBUGGER=" + (bEnableDeprecatedDebugger ? "1" : "0"));

            // include for compatibility with old debugger
            // to be removed with next version
			{
                PublicIncludePaths.AddRange(
					new string[] {
						"Developer/GameplayDebugger/Public",
						"Developer/AIModule/Public",
						"Developer/Settings/Public",
						"Runtime/GameplayAbilities/Public",
						// ... add public include paths required here ...
					}
                );

				PrivateIncludePaths.AddRange(
					new string[] {
						"Runtime/Engine/Private",
						"Runtime/AIModule/Private",
						// ... add other private include paths required here ...
						}
					);

                PrivateDependencyModuleNames.AddRange(new string[] { "AIModule", "GameplayTasks" });
                CircularlyReferencedDependentModules.AddRange(new string[] { "AIModule", "GameplayTasks" });

				DynamicallyLoadedModuleNames.AddRange(
                new string[]
				    {
					    // Removed direct dependency on GameplayAbilities from GameplayDebugger to solve problems with its classes showing in blueprints editor as a result of the OS automatically loading the library
					    "GameplayAbilities",
					}
				);
			}
        }
    }
}
