// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GpgCppSDK : ModuleRules
{
	public GpgCppSDK(TargetInfo Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string GPGAndroidPath = Path.Combine(UEBuildConfiguration.UEThirdPartySourceDirectory, "GooglePlay/gpg-cpp-sdk.v2.1/gpg-cpp-sdk/android/");

			PublicIncludePaths.Add(Path.Combine(GPGAndroidPath, "include/"));
			PublicLibraryPaths.Add(Path.Combine(GPGAndroidPath, "lib/gnustl/armeabi-v7a/"));
			PublicLibraryPaths.Add(Path.Combine(GPGAndroidPath, "lib/gnustl/arm64-v8a"));
			PublicLibraryPaths.Add(Path.Combine(GPGAndroidPath, "lib/gnustl/x86/"));

			PublicAdditionalLibraries.Add("gpg");
		}
	}
}
