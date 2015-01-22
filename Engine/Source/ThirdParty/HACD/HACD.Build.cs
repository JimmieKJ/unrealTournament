// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class HACD : ModuleRules
{
    public HACD(TargetInfo Target)
	{
		Type = ModuleType.External;

		string HACDDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "HACD/HACD_1.0/";
		string HACDLibPath = HACDDirectory;
		PublicIncludePaths.Add(HACDDirectory + "public");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			HACDLibPath = HACDLibPath + "lib/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(HACDLibPath);

            if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add("HACDd_64.lib");
            }
            else
            {
                PublicAdditionalLibraries.Add("HACD_64.lib");
            }
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			HACDLibPath = HACDLibPath + "lib/Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(HACDLibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("HACDd.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("HACD.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibPath = HACDDirectory + "lib/Mac/";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(LibPath + "libhacd_debug.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(LibPath + "libhacd.a");
			}
		}
	}
}

