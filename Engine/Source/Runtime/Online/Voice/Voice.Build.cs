// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Voice : ModuleRules
{
	public Voice(TargetInfo Target)
	{
		Definitions.Add("VOICE_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Online/Voice/Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core"
			}
			);

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{
			AddThirdPartyPrivateStaticDependencies(Target, "DirectSound");
		}
		else if(Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicFrameworks.AddRange(new string[] { "CoreAudio", "AudioUnit", "AudioToolbox" });
		}

		AddThirdPartyPrivateStaticDependencies(Target, "libOpus");
    }
}


		



		
