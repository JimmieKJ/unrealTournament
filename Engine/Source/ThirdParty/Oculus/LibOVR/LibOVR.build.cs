// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibOVR : ModuleRules
{
	public LibOVR(TargetInfo Target)
	{
		/** Mark the current version of the Oculus SDK */
		string LibOVRVersion = "_04";
		Type = ModuleType.External;

		string OculusThirdPartyDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/LibOVR/LibOVR" + LibOVRVersion;

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{

            PublicIncludePaths.Add(OculusThirdPartyDirectory + "/Include");

            string LibraryPath = OculusThirdPartyDirectory + "/Lib/";
			string LibraryName = "libovr";
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryPath += "x64/";
				LibraryName += "64";
			}
            else if (Target.Platform == UnrealTargetPlatform.Win32)
            {
                LibraryPath += "Win32/";
            }

			//LibraryName += "_sp";

			LibraryPath += "VS" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");

			PublicAdditionalLibraries.Add("Wtsapi32.lib");
            //PublicAdditionalLibraries.Add(LibraryName + "d.lib");
			//PublicDelayLoadDLLs.Add(LibraryName + ".dll");
		}
		else if ((Target.Platform == UnrealTargetPlatform.Mac))
		{
            PublicIncludePaths.Add(OculusThirdPartyDirectory + "/Include");

            string LibraryPath = OculusThirdPartyDirectory + "/Lib/MacOS/Release/";
			string LibraryName = "libovr";
			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryPath + LibraryName + ".a");
		}
	}
}
