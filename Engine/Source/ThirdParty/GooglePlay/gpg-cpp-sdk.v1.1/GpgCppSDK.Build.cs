// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GpgCppSDK : ModuleRules
{
	public GpgCppSDK(TargetInfo Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string GPGAndroidPath = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "GooglePlay/gpg-cpp-sdk.v1.1/gpg-cpp-sdk/android/");

			PublicIncludePaths.Add(Path.Combine(GPGAndroidPath, "include/"));
			PublicLibraryPaths.Add(Path.Combine(GPGAndroidPath, "lib/gnustl/ARMv7/"));
			PublicLibraryPaths.Add(Path.Combine(GPGAndroidPath, "lib/gnustl/x86/"));

			PublicAdditionalLibraries.Add("gpg");
		}
	}
}
