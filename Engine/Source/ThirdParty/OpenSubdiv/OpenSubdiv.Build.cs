// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenSubdiv : ModuleRules
{
	public OpenSubdiv(TargetInfo Target)
	{
		Type = ModuleType.External;

		// Compile and link with OpenSubDiv
        string OpenSubdivPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "OpenSubdiv/3.0.2";

		PublicIncludePaths.Add( OpenSubdivPath + "/opensubdiv" );

		// @todo SubDSurface: Support other platforms
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            string LibFolder = "";
            {
                switch (WindowsPlatform.Compiler)
                {
                    case WindowsCompiler.VisualStudio2013: LibFolder = "/libVS2013"; break;
                    case WindowsCompiler.VisualStudio2015: LibFolder = "/libVS2015"; break;
                }
            }

            if (LibFolder != "")
            {
                bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT);
                string ConfigFolder = bDebug ? "/Debug" : "/RelWithDebInfo";

                PublicLibraryPaths.Add(OpenSubdivPath + LibFolder + ConfigFolder);

                PublicAdditionalLibraries.Add("osd_cpu_obj.lib");
                PublicAdditionalLibraries.Add("sdc_obj.lib");
                PublicAdditionalLibraries.Add("vtr_obj.lib");
                PublicAdditionalLibraries.Add("far_obj.lib");
            }
        }
	}
}