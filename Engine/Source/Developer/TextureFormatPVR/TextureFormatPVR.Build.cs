// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatPVR : ModuleRules
{
	public TextureFormatPVR( TargetInfo Target )
	{
        PrivateIncludePathModuleNames.AddRange(new string[] { "TargetPlatform", "TextureCompressor", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "ImageCore", "ImageWrapper" });

		//if (UEBuildConfiguration.bCompileLeanAndMeanUE == false)
		//{
		//	PrivateDependencyModuleNames.Add("nvTextureTools");
		//}
	}
}