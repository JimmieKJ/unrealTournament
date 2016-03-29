// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ALAudio : ModuleRules
{
    public ALAudio(TargetInfo Target)
    {
        PrivateIncludePathModuleNames.Add("TargetPlatform");

        PrivateDependencyModuleNames.AddRange(new string[]
        {
			"Core",
			"CoreUObject",
			"Engine",
		});

        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenAL");
        AddEngineThirdPartyPrivateStaticDependencies(Target,
            "OpenAL",
            "UEOgg",
            "Vorbis",
            "VorbisFile"
        );

		PrecompileForTargets = PrecompileTargetsType.None;
    }
}
