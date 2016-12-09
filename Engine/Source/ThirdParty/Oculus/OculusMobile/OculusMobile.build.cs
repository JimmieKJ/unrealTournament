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

		string SdkVersion = "SDK_" + SDK_Product + "_" + SDK_Major + "_" + SDK_Minor;
		string SourceDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/OculusMobile/" + SdkVersion + "/";
		string BinariesDirectory = UEBuildConfiguration.UEThirdPartyBinariesDirectory + "Oculus/OculusMobile/" + SdkVersion + "/";

		PublicSystemIncludePaths.Add(SourceDirectory + "Include");
		PublicSystemIncludePaths.Add(SourceDirectory + "Include/VrApi");
		PublicSystemIncludePaths.Add(SourceDirectory + "Include/Kernel");
		PublicSystemIncludePaths.Add(SourceDirectory + "Include/SystemUtils");

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicLibraryPaths.Add(SourceDirectory + "Libs/armeabi-v7a/");

			PublicAdditionalLibraries.Add("vrapi");
			PublicAdditionalLibraries.Add("ovrkernel");
			PublicAdditionalLibraries.Add("systemutils");
			PublicAdditionalLibraries.Add("openglloader");
		}
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicLibraryPaths.Add(SourceDirectory + "Libs/Win64/");
			PublicAdditionalLibraries.Add("VrApiShim.lib");
			PublicDelayLoadDLLs.Add("VrApiShim.dll");
			RuntimeDependencies.Add(new RuntimeDependency(BinariesDirectory + "Win64/VrApiShim.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32 )
		{
			PublicLibraryPaths.Add(SourceDirectory + "Libs/Win32/");
			PublicAdditionalLibraries.Add("VrApiShim.lib");
			PublicDelayLoadDLLs.Add("VrApiShim.dll");
			RuntimeDependencies.Add(new RuntimeDependency(BinariesDirectory + "Win32/VrApiShim.dll"));
		}
	}
}
