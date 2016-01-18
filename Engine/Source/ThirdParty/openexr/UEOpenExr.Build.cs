// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UEOpenEXR : ModuleRules
{
    public UEOpenEXR(TargetInfo Target)
    {
        Type = ModuleType.External;
        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac)
        {
            bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT);
            string LibDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "openexr/Deploy/lib/";
            string Platform;
            switch (Target.Platform)
            {
                case UnrealTargetPlatform.Win64:
                    Platform = "x64";
                    LibDir += "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
                    break;
                case UnrealTargetPlatform.Win32:
                    Platform = "Win32";
                    LibDir += "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
                    break;
                case UnrealTargetPlatform.Mac:
                    Platform = "Mac";
                    bDebug = false;
                    break;
                default:
                    return;
            }
            LibDir = LibDir + "/" + Platform;
            LibDir = LibDir + "/Static" + (bDebug ? "Debug" : "Release");
            PublicLibraryPaths.Add(LibDir);

			if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						"Half.lib",
						"Iex.lib",
						"IlmImf.lib",
						"IlmThread.lib",
						"Imath.lib",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.AddRange(
					new string[] {
						LibDir + "/libHalf.a",
						LibDir + "/libIex.a",
						LibDir + "/libIlmImf.a",
						LibDir + "/libIlmThread.a",
						LibDir + "/libImath.a",
					}
				);
			}

            PublicSystemIncludePaths.AddRange(
                new string[] {
                    UEBuildConfiguration.UEThirdPartySourceDirectory + "openexr/Deploy/include",
			    }
            );
        }
    }
}

