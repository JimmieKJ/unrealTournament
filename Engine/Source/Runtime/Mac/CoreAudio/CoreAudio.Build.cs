// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

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
	}
}
