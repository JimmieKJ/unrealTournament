// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTML5Audio : ModuleRules
{
	public HTML5Audio(TargetInfo Target)
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
        "UEOgg",
        "Vorbis",
        "VorbisFile"
        );
	}
}
