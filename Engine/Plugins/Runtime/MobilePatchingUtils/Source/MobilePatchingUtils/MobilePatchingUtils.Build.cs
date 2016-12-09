// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class MobilePatchingUtils : ModuleRules
    {
        public MobilePatchingUtils(TargetInfo Target)
        {
            PrivateIncludePaths.Add("MobilePatchingUtils/Private");

            PublicDependencyModuleNames.AddRange(new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
            });

            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "PakFile",
                "HTTP",
                "BuildPatchServices"
            });
        }
    }
}
