// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Eigen : ModuleRules
{
    public Eigen(TargetInfo Target)
    {
        Type = ModuleType.External;
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac)
        {
           PublicIncludePaths.Add( ModuleDirectory + "/Eigen/" );
        }

        Definitions.Add("EIGEN_MPL2_ONLY");
    }
}
