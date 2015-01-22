// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

        AddThirdPartyPrivateStaticDependencies(Target, "OpenAL");
        AddThirdPartyPrivateStaticDependencies(Target,
            "OpenAL",
            "UEOgg",
            "Vorbis",
            "VorbisFile"
        );
    }
}
