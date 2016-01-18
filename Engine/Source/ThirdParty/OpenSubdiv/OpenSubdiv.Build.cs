// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenSubdiv : ModuleRules
{
	public OpenSubdiv(TargetInfo Target)
	{
		Type = ModuleType.External;

		// Compile and link with kissFFT
		string OpenSubdivPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "OpenSubdiv/3.0.0";

		PublicIncludePaths.Add( OpenSubdivPath + "/opensubdiv" );

		// @todo subdiv: Add other platforms and debug builds
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT);

            if (bDebug)
            {
                PublicLibraryPaths.Add(OpenSubdivPath + "/lib/Debug");
            }
            else
            {
                PublicLibraryPaths.Add(OpenSubdivPath + "/lib/RelWithDebInfo");
            }

            PublicAdditionalLibraries.Add("osd_cpu_obj.lib");
            PublicAdditionalLibraries.Add("sdc_obj.lib");
            PublicAdditionalLibraries.Add("vtr_obj.lib");
            PublicAdditionalLibraries.Add("far_obj.lib");
        }
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
		}
	}
}
