// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatDXT : ModuleRules
{
	public TextureFormatDXT(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"TargetPlatform",
				"TextureCompressor",
				"Engine"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ImageCore",
				"ImageWrapper"
			}
			);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "nvTextureTools");
	}
}
