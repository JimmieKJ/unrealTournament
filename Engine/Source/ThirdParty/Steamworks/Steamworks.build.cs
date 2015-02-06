// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class Steamworks : ModuleRules
{
	public Steamworks(TargetInfo Target)
	{
		/** Mark the current version of the Steam SDK */
		string SteamVersion = "v130";
		Type = ModuleType.External;

		string SdkBase = UEBuildConfiguration.UEThirdPartySourceDirectory + "Steamworks/Steam" + SteamVersion + "/sdk";
		if (!Directory.Exists(SdkBase))
		{
			string Err = string.Format("steamworks SDK not found in {0}", SdkBase);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}

		PublicIncludePaths.Add(SdkBase + "/public");

		string LibraryPath = SdkBase + "/redistributable_bin/";
		string LibraryName = "steam_api";

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryPath += "win64";
				LibraryName += "64";
			}
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");
			PublicDelayLoadDLLs.Add(LibraryName + ".dll");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			LibraryPath += "osx32/libsteam_api.dylib";
			PublicDelayLoadDLLs.Add(LibraryPath);
			PublicAdditionalShadowFiles.Add(LibraryPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			LibraryPath += "linux64";
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName);
		}
	}
}
