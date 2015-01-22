// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateCore : ModuleRules
{
	public SlateCore(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"InputCore",
				"Json",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/SlateCore/Private",
				"Runtime/SlateCore/Private/Animation",
				"Runtime/SlateCore/Private/Application",
				"Runtime/SlateCore/Private/Brushes",
				"Runtime/SlateCore/Private/Commands",
				"Runtime/SlateCore/Private/Fonts",
				"Runtime/SlateCore/Private/Input",
				"Runtime/SlateCore/Private/Layout",
				"Runtime/SlateCore/Private/Logging",
				"Runtime/SlateCore/Private/Rendering",
				"Runtime/SlateCore/Private/Sound",
				"Runtime/SlateCore/Private/Styling",
				"Runtime/SlateCore/Private/Textures",
				"Runtime/SlateCore/Private/Types",
				"Runtime/SlateCore/Private/Widgets",
			}
		);

		if (Target.Type.Value != TargetRules.TargetType.Server && UEBuildConfiguration.bCompileFreeType)
		{
			AddThirdPartyPrivateStaticDependencies(Target, "FreeType2");
			Definitions.Add("WITH_FREETYPE=1");
		}
		else
		{
			Definitions.Add("WITH_FREETYPE=0");
		}

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddThirdPartyPrivateStaticDependencies(Target, "XInput");
		}
	}
}
