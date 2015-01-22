// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class zlib : ModuleRules
{
	public zlib(TargetInfo Target)
	{
		Type = ModuleType.External;

		string zlibPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "zlib/zlib-1.2.5/";

		PublicIncludePaths.Add(zlibPath + "Inc");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(zlibPath + "Lib/Win64");
			PublicAdditionalLibraries.Add("zlib_64.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 || (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32"))
		{
			PublicLibraryPaths.Add(zlibPath + "Lib/Win32");
			PublicAdditionalLibraries.Add("zlib.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac ||
				 Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalLibraries.Add("z");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add("z");
		}
        else if (Target.Platform == UnrealTargetPlatform.HTML5)
        {
			if (UEBuildConfiguration.bCompileForSize)
			{
				PublicAdditionalLibraries.Add(zlibPath + "Lib/HTML5/zlib_Oz.bc");
			}
			else
			{
				PublicAdditionalLibraries.Add(zlibPath + "Lib/HTML5/zlib.bc");
			}
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            if (Target.IsMonolithic)
            {
                PublicAdditionalLibraries.Add(zlibPath + "Lib/Linux/" + Target.Architecture + "/libz.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(zlibPath + "Lib/Linux/" + Target.Architecture + "/libz_fPIC.a");
            }
        }
    }
}
