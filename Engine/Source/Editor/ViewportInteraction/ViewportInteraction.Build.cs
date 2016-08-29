// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
    public class ViewportInteraction : ModuleRules
    {
        public ViewportInteraction(TargetInfo Target)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[] {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "UnrealEd",
                    "Slate",
                    "SlateCore",
                    "HeadMountedDisplay"
                }
            );

            PrivateIncludePathModuleNames.AddRange(
                new string[] {
                }
            );

            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                }
            );
        }
    }
}