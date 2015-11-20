// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CryptoPP : ModuleRules
{
    public CryptoPP(TargetInfo Target)
	{
		Type = ModuleType.External;

		string LibFolder = "lib/";
		string LibPrefix = "";
        string LibPostfixAndExt = ".";//(Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d." : ".";
        string CryptoPPPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "CryptoPP/5.6.2/";

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicIncludePaths.Add(CryptoPPPath + "include");
            PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory);
            LibFolder += "Win64/VS2013/";
            LibPostfixAndExt += "lib";
            PublicLibraryPaths.Add(CryptoPPPath + LibFolder);
        }

        PublicAdditionalLibraries.Add(LibPrefix + "cryptlib" + LibPostfixAndExt);
	}

}