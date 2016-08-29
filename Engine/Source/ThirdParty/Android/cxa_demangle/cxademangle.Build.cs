// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class cxademangle : ModuleRules
{
	public cxademangle(TargetInfo Target)
	{
		Type = ModuleType.External;
		string cxademanglepath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Android/cxa_demangle/";

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(cxademanglepath + "armeabi-v7a");
			PublicLibraryPaths.Add(cxademanglepath + "arm64-v8a");
			PublicLibraryPaths.Add(cxademanglepath + "x86");
			PublicLibraryPaths.Add(cxademanglepath + "x64");

			PublicAdditionalLibraries.Add("cxa_demangle");
		}
    }
}
