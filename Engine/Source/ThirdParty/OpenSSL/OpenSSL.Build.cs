// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenSSL : ModuleRules
{
	public OpenSSL(TargetInfo Target)
	{
		Type = ModuleType.External;

		string LibFolder = "lib/";
		string LibPrefix = "";
		string LibPostfixAndExt = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d." : ".";
		string OpenSSLPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "OpenSSL/1.0.1g/";
		
		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			LibPostfixAndExt = ".a";
			LibFolder += "Mac/";
			LibPrefix = OpenSSLPath + LibFolder;
			PublicIncludePaths.Add(OpenSSLPath + "include");
			PublicLibraryPaths.Add(OpenSSLPath + LibFolder);
			PublicAdditionalLibraries.Add(LibPrefix + "libssl" + LibPostfixAndExt);
			PublicAdditionalLibraries.Add(LibPrefix + "libcrypto" + LibPostfixAndExt);
		}
		else
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicIncludePaths.Add(OpenSSLPath + "include");
				LibFolder += "Win64/VS2013/";
				LibPostfixAndExt += "lib";
				PublicLibraryPaths.Add(OpenSSLPath + LibFolder);
			}

			PublicAdditionalLibraries.Add(LibPrefix + "libeay32" + LibPostfixAndExt);
			PublicAdditionalLibraries.Add(LibPrefix + "ssleay32" + LibPostfixAndExt);

			PublicDelayLoadDLLs.AddRange(
						   new string[] {
							"libeay32.dll", 
							"ssleay32.dll" 
					   }
					   );
		}
	}
}