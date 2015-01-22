// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEOpenEXR : ModuleRules
{
    public UEOpenEXR(TargetInfo Target)
    {
        Type = ModuleType.External;
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
        {
            bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT);
            string LibDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "openexr/Deploy/lib/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
            string Platform;
            switch (Target.Platform)
            {
                case UnrealTargetPlatform.Win64:
                    Platform = "x64";
                    break;
                case UnrealTargetPlatform.Win32:
                    Platform = "Win32";
                    break;
                case UnrealTargetPlatform.Mac:
                    Platform = "Mac";
                    break;
                default:
                    return;
            }
            LibDir = LibDir + "/" + Platform;
            LibDir = LibDir + "/Static" + (bDebug ? "Debug" : "Release");
            PublicLibraryPaths.Add(LibDir);

            PublicAdditionalLibraries.AddRange(
                new string[] {
                    "Half.lib",
                    "Iex.lib",
                    "IlmImf.lib",
                    "IlmThread.lib",
                    "Imath.lib",
			    }
            );

            PublicIncludePaths.AddRange(
                new string[] {
                    UEBuildConfiguration.UEThirdPartySourceDirectory + "openexr/Deploy/include",
			    }
            );
        }
    }
}

