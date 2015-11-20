// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CoreAudio : ModuleRules
{
	public CoreAudio(TargetInfo Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
			}
			);

		AddThirdPartyPrivateStaticDependencies(Target, 
			"UEOgg",
			"Vorbis",
			"VorbisFile"
			);

		PublicFrameworks.AddRange(new string[] { "CoreAudio", "AudioUnit", "AudioToolbox" });

		AdditionalBundleResources.Add(new UEBuildBundleResource("ThirdParty/Mac/RadioEffectUnit/RadioEffectUnit.component"));

		// Add contents of component directory as runtime dependencies
		foreach (string FilePath in Directory.EnumerateFiles(UEBuildConfiguration.UEThirdPartySourceDirectory + "Mac/RadioEffectUnit/RadioEffectUnit.component", "*", SearchOption.AllDirectories))
		{
			RuntimeDependencies.Add(new RuntimeDependency(FilePath));
		}
	}
}
