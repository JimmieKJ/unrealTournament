// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatAndroid : ModuleRules
{
	public TextureFormatAndroid(TargetInfo Target)
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

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddThirdPartyPrivateStaticDependencies(Target, "QualcommTextureConverter");
		}
		else
		{
			AddThirdPartyPrivateStaticDependencies(Target, "QualcommTextureConverter");
		}

	}
}
