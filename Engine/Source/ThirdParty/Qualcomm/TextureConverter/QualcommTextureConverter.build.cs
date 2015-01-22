// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class QualcommTextureConverter : ModuleRules
{
	public QualcommTextureConverter(TargetInfo Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Qualcomm/TextureConverter/Include");

			string LibraryPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Qualcomm/TextureConverter/Lib/";
			string LibraryName = "TextureConverter";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
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

			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");
		}
	}
}
