// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class Steamworks : ModuleRules
{
	public Steamworks(TargetInfo Target)
	{
		/** Mark the current version of the Steam SDK */
		string SteamVersion = "v132";
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

		if(Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");
			PublicDelayLoadDLLs.Add(LibraryName + ".dll");

			string SteamBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/Steamworks/Steam{0}/Win32/", SteamVersion);
			RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + "steam_api.dll"));

			if(Target.Type == TargetRules.TargetType.Server)
			{
				RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + "steamclient.dll"));
				RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + "tier0_s.dll"));
				RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + "vstdlib_s.dll"));
			}
			else
			{
				// assume SteamController is needed
				RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Config/controller.vdf"));
			}
		}
		else if(Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(LibraryPath + "win64");
			PublicAdditionalLibraries.Add(LibraryName + "64.lib");
			PublicDelayLoadDLLs.Add(LibraryName + "64.dll");

			string SteamBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/Steamworks/Steam{0}/Win64/", SteamVersion);
			RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + LibraryName + "64.dll"));

			if(Target.Type == TargetRules.TargetType.Server)
			{
				RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + "steamclient64.dll"));
				RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + "tier0_s64.dll"));
				RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + "vstdlib_s64.dll"));
			}
			else
			{
				// assume SteamController is needed
				RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Config/controller.vdf"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			LibraryPath += "osx32/libsteam_api.dylib";
			PublicDelayLoadDLLs.Add(LibraryPath);
			PublicAdditionalShadowFiles.Add(LibraryPath);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			if (Target.IsMonolithic)
			{
				LibraryPath += "linux64";
				PublicLibraryPaths.Add(LibraryPath);
				PublicAdditionalLibraries.Add(LibraryName);
			}
			else
			{
				LibraryPath += "linux64/libsteam_api.so";
				PublicDelayLoadDLLs.Add(LibraryPath);
			}

			string SteamBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/Steamworks/Steam{0}/Linux/", SteamVersion);
			RuntimeDependencies.Add(new RuntimeDependency(SteamBinariesDir + "libsteam_api.so"));

			if (Target.Type != TargetRules.TargetType.Server)
			{
				// assume SteamController is needed
				RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Config/controller.vdf"));
			}
		}
	}
}
