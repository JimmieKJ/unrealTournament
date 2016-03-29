// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System;
using System.IO;

public class HarfBuzz : ModuleRules
{
	public HarfBuzz(TargetInfo Target)
	{
		Type = ModuleType.External;
		
		// Can't be used without our dependencies
		if (!UEBuildConfiguration.bCompileFreeType || !UEBuildConfiguration.bCompileICU)
		{
			Definitions.Add("WITH_HARFBUZZ=0");
			return;
		}

		string HarfBuzzVersion = "harfbuzz-1.0.5";
		string HarfBuzzRootPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "HarfBuzz/" + HarfBuzzVersion + "/";

		// Includes
		PublicSystemIncludePaths.Add(HarfBuzzRootPath + "src" + "/");

		string PlatformFolderName = Target.Platform.ToString();

        string HarfBuzzLibPath = HarfBuzzRootPath + PlatformFolderName + "/";
        if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
        {
            HarfBuzzLibPath = HarfBuzzRootPath + "Win32/";
        }

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32) || 
            (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
			Definitions.Add("WITH_HARFBUZZ=1");
			
			string VSVersionFolderName = "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			HarfBuzzLibPath += VSVersionFolderName + "/";

			string BuildTypeFolderName = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				? "Debug"
				: "RelWithDebInfo";
			HarfBuzzLibPath += BuildTypeFolderName + "/";

			PublicLibraryPaths.Add(HarfBuzzLibPath);
			PublicAdditionalLibraries.Add("harfbuzz.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			Definitions.Add("WITH_HARFBUZZ=1");

			PublicAdditionalLibraries.Add(HarfBuzzLibPath + "libharfbuzz.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.PS4)
		{
			Definitions.Add("WITH_HARFBUZZ=1");

			string BuildTypeFolderName = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
				? "Debug"
				: "Release";
			HarfBuzzLibPath += BuildTypeFolderName + "/";

			PublicLibraryPaths.Add(HarfBuzzLibPath);
			PublicAdditionalLibraries.Add("harfbuzz"); // Automatically transforms to libharfbuzz.a
		}
		else
		{
			Definitions.Add("WITH_HARFBUZZ=0");
		}
	}
}
