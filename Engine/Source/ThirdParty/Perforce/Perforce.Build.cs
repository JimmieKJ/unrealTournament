// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Perforce : ModuleRules
{
	public Perforce(TargetInfo Target)
	{
		Type = ModuleType.External;

		string LibFolder = "lib/";
		string LibPrefix = "";
		string LibPostfixAndExt = ".";
		string P4APIPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Perforce/p4api-2012.1/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibFolder += "win64";
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			LibFolder += "win32";
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			P4APIPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Perforce/p4api-2014.1/";
			LibFolder += "mac";
		}
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            P4APIPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Perforce/p4api-2014.1/" ;
            LibFolder += "linux/" + Target.Architecture;
        }

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win64)
        {
            if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
                LibPostfixAndExt = "d.";
            if (WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2013)
            {
                P4APIPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Perforce/p4api-2014.2/";
            }
            else
            {
                P4APIPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Perforce/p4api-2013.1-BETA/";
            }
            LibPostfixAndExt += "lib";
            PublicLibraryPaths.Add(P4APIPath + LibFolder);
        }
        else
        {
            LibPrefix = P4APIPath + LibFolder + "/";
            LibPostfixAndExt = ".a";
        }

        PublicSystemIncludePaths.Add(P4APIPath + "include");
        PublicAdditionalLibraries.Add(LibPrefix + "libclient" + LibPostfixAndExt);

        if (Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(LibPrefix + "libp4sslstub" + LibPostfixAndExt);
        }

        PublicAdditionalLibraries.Add(LibPrefix + "librpc" + LibPostfixAndExt);
        PublicAdditionalLibraries.Add(LibPrefix + "libsupp" + LibPostfixAndExt);
	}
}
