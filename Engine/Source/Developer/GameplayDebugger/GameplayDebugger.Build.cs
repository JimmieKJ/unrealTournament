// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class GameplayDebugger : ModuleRules
    {
        public GameplayDebugger(TargetInfo Target)
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
					"Developer/GameplayDebugger/Private",
                    "Runtime/Engine/Private",
                    "Runtime/AIModule/Private",
					// ... add other private include paths required here ...
				    }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
				{
					"Core",
					"CoreUObject",
                    "InputCore",
					"Engine",    
                    "RenderCore",
                    "RHI",
                    "ShaderCore",
                    "Settings",
                    "AIModule",  // it have to be here for now. It'll be changed to remove any dependency to AIModule in future
                    "GameplayAbilities",
				}
                );

            DynamicallyLoadedModuleNames.AddRange(
                new string[]
				    {
					    // ... add any modules that your module loads dynamically here ...
				    }
                );

            if (UEBuildConfiguration.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
                PrivateDependencyModuleNames.Add("LevelEditor");
                PrivateDependencyModuleNames.Add("Slate");
                PublicIncludePaths.Add("Editor/LevelEditor/Public");
            }

            if (UEBuildConfiguration.bCompileRecast)
            {
                PrivateDependencyModuleNames.Add("Navmesh");
            }
        }
    }
}
