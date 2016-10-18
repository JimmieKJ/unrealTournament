// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class OpenVR : ModuleRules
{
	public OpenVR(TargetInfo Target)
	{
		/** Mark the current version of the OpenVR SDK */
		string OpenVRVersion = "v1_0_2";
		Type = ModuleType.External;

		string SdkBase = UEBuildConfiguration.UEThirdPartySourceDirectory + "OpenVR/OpenVR" + OpenVRVersion;
		if (!Directory.Exists(SdkBase))
		{
			string Err = string.Format("OpenVR SDK not found in {0}", SdkBase);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}

		PublicIncludePaths.Add(SdkBase + "/headers");

		string LibraryPath = SdkBase + "/lib/";

		if(Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicLibraryPaths.Add(LibraryPath + "win32");
			PublicAdditionalLibraries.Add("openvr_api.lib");
			PublicDelayLoadDLLs.Add("openvr_api.dll");

			string OpenVRBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/OpenVR/OpenVR{0}/Win32/", OpenVRVersion);
			RuntimeDependencies.Add(new RuntimeDependency(OpenVRBinariesDir + "openvr_api.dll"));
		}
		else if(Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(LibraryPath + "win64");
			PublicAdditionalLibraries.Add("openvr_api.lib");
			PublicDelayLoadDLLs.Add("openvr_api.dll");

			string OpenVRBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/OpenVR/OpenVR{0}/Win64/", OpenVRVersion);
			RuntimeDependencies.Add(new RuntimeDependency(OpenVRBinariesDir + "openvr_api.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string DylibPath = SdkBase + "/bin/osx32/libopenvr_api.dylib";
			PublicDelayLoadDLLs.Add(DylibPath);
			PublicAdditionalShadowFiles.Add(DylibPath);

			string OpenVRBinariesDir = String.Format("$(EngineDir)/Binaries/ThirdParty/OpenVR/OpenVR{0}/osx32/", OpenVRVersion);
			RuntimeDependencies.Add(new RuntimeDependency(OpenVRBinariesDir + "libopenvr_api.dylib"));
		}
	}
}
