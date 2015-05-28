// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
// Not really Epic, but Freenode #ue4linux's RushPL and amigo 
using UnrealBuildTool;

public class LinuxNativeDialogs : ModuleRules
{
	public LinuxNativeDialogs(TargetInfo Target)
	{
		Type = ModuleType.External;

		string LNDPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "LinuxNativeDialogs/UELinuxNativeDialogs/";
		string LNDLibPath = LNDPath + "lib/";
		
		Definitions.Add("WITH_LINUX_NATIVE_DIALOGS=1");

		if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			PublicLibraryPaths.Add(LNDLibPath + "Linux/" + Target.Architecture);
			PublicIncludePaths.Add(LNDPath + "include/");
			PublicAdditionalLibraries.Add("LND");
		}
	}
}
