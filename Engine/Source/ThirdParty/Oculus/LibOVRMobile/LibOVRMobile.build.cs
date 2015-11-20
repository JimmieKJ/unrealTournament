// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibOVRMobile : ModuleRules
{
	public LibOVRMobile(TargetInfo Target)
	{
		// current version of the Mobile Oculus SDK
		string LibOVRVersion = "_061";
		Type = ModuleType.External;

		string OculusThirdPartyDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/LibOVRMobile/LibOVRMobile" + LibOVRVersion;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(OculusThirdPartyDirectory + "/VrApi/Libs/Android/armeabi-v7a/");

			PublicAdditionalLibraries.Add("vrapi");

			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/VrApi/Include");
			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/LibOVRKernel/Include");
			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/LibOVRKernel/Src");
		}
	}
}
