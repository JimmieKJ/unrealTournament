// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OculusMobile : ModuleRules
{
	// Version of VrApi
	public const int SDK_Product= 1;
	public const int SDK_Major 	= 0;
	public const int SDK_Minor 	= 3;

	public OculusMobile(TargetInfo Target)
	{
		// current version of the OculusMobile Oculus SDK
		Type = ModuleType.External;

		string VrApiVersion = SDK_Product + "_" + SDK_Major + "_" + SDK_Minor;
		string OculusThirdPartyDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/OculusMobile/SDK_" + VrApiVersion;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(OculusThirdPartyDirectory + "/Libs/armeabi-v7a/");

			PublicAdditionalLibraries.Add("vrapi");
			PublicAdditionalLibraries.Add("ovrkernel");
			PublicAdditionalLibraries.Add("systemutils");
			PublicAdditionalLibraries.Add("openglloader");

			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/Include/VrApi");
			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/Include/Kernel");
			PublicSystemIncludePaths.Add(OculusThirdPartyDirectory + "/Include/SystemUtils");
		}
	}
}
