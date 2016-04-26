// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatPVR : ModuleRules
{
	public TextureFormatPVR( TargetInfo Target )
	{
        PrivateIncludePathModuleNames.AddRange(new string[] { "TargetPlatform", "TextureCompressor", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "ImageCore", "ImageWrapper" });

		if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/ImgTec/PVRTexToolCLI.exe"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac /*|| Target.Platform == UnrealTargetPlatform.Linux*/)
		{
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/ImgTec/PVRTexToolCLI"));
		}

		//if (UEBuildConfiguration.bCompileLeanAndMeanUE == false)
		//{
		//	PrivateDependencyModuleNames.Add("nvTextureTools");
		//}
	}
}