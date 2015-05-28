// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class VHACD : ModuleRules
{
    public VHACD(TargetInfo Target)
	{
		Type = ModuleType.External;

		string VHACDDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "VHACD/";
        string VHACDLibPath = VHACDDirectory;
        PublicIncludePaths.Add(VHACDDirectory + "public");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            VHACDLibPath = VHACDLibPath + "lib/Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
            PublicLibraryPaths.Add(VHACDLibPath);

            if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add("VHACDd.lib");
            }
            else
            {
                PublicAdditionalLibraries.Add("VHACD.lib");
            }
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string LibPath = VHACDDirectory + "lib/Mac/";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(LibPath + "libVHACD_LIBd.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(LibPath + "libVHACD_LIB.a");
			}
			PublicFrameworks.AddRange(new string[] { "OpenCL" });
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			if (Target.IsMonolithic)
			{
				PublicAdditionalLibraries.Add(VHACDDirectory + "Lib/Linux/" + Target.Architecture + "/libVHACD.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(VHACDDirectory + "Lib/Linux/" + Target.Architecture + "/libVHACD_fPIC.a");
			}
		}
	}
}

