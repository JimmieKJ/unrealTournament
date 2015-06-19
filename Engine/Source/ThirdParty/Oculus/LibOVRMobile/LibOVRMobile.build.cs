// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibOVRMobile : ModuleRules
{
	public LibOVRMobile(TargetInfo Target)
	{
		// current version of the Mobile Oculus SDK
		string LibOVRVersion = "_050";
		Type = ModuleType.External;

		string OculusThirdPartyDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/LibOVRMobile/LibOVRMobile" + LibOVRVersion;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(OculusThirdPartyDirectory + "/lib/armv7");

			PublicAdditionalLibraries.Add("oculus");

			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/VRLib/jni/VrApi");
			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/VRLib/jni/LibOVR/Include");
			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/VRLib/jni/LibOVR/Src");
		}
	}
}
