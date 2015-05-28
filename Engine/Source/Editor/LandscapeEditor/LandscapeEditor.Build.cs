// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LandscapeEditor : ModuleRules
{
	public LandscapeEditor(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"Engine",
				"Landscape",
				"RenderCore",
                "InputCore",
				"UnrealEd",
				"PropertyEditor",
				"ImageWrapper",
                "EditorWidgets",
                "Foliage",
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"DesktopPlatform",
				"ContentBrowser",
                "AssetTools",
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
				"DesktopPlatform",
				"ImageWrapper",
			}
			);

		// KissFFT is used by the smooth tool.
		if (UEBuildConfiguration.bCompileLeanAndMeanUE == false &&
			(Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32 || Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.Linux))
		{
			AddThirdPartyPrivateStaticDependencies(Target, "Kiss_FFT");
		}
		else
		{
			Definitions.Add("WITH_KISSFFT=0");
		}
	}
}
