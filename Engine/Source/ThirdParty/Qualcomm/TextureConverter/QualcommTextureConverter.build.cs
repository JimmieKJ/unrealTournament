// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class QualcommTextureConverter : ModuleRules
{
	public QualcommTextureConverter(TargetInfo Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32) ||
			(Target.Platform == UnrealTargetPlatform.Mac))
		{
			PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Qualcomm/TextureConverter/Include");

			string LibraryPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Qualcomm/TextureConverter/Lib/";
			string LibraryName = "TextureConverter";
			string LibraryExtension = ".lib";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT && Target.Platform != UnrealTargetPlatform.Mac)
			{
				LibraryName += "_d";
			}

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryPath += "vs" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/x64";
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				LibraryPath += "vs" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/Win32";
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				LibraryPath += "osx64";
				LibraryExtension = ".dylib";
				LibraryName = LibraryPath + "/lib" + LibraryName;
			}

			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + LibraryExtension);
		}
	}
}
