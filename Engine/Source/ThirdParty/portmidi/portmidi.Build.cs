// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class portmidi : ModuleRules
{
	public portmidi(TargetInfo Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "portmidi/include");

        if (Target.Platform == UnrealTargetPlatform.Win32)
        {
            PublicLibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "portmidi/lib/Win32");
            PublicAdditionalLibraries.Add("portmidi.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicLibraryPaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "portmidi/lib/Win64");
            PublicAdditionalLibraries.Add("portmidi_64.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "portmidi/lib/Mac/libportmidi.a");
			PublicAdditionalFrameworks.Add( new UEBuildFramework( "CoreAudio" ));
			PublicAdditionalFrameworks.Add( new UEBuildFramework( "CoreMIDI" ));
        }
	}
}
