// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Leap : ModuleRules
{
    public Leap(TargetInfo Target)
	{
		Type = ModuleType.External;

		if ((Target.Platform == UnrealTargetPlatform.Win64)
            || (Target.Platform == UnrealTargetPlatform.Win32))
		{
            PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Leap/include");

            string LibraryPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Leap/lib";
			string LibraryName = "Leap";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				LibraryName += "d";
			}

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryPath += "/x64";
			}
			else if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				LibraryPath += "/x86";
			}

			PublicLibraryPaths.Add(LibraryPath);
			PublicAdditionalLibraries.Add(LibraryName + ".lib");
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Leap/" + Target.Platform.ToString() + "/" + LibraryName + ".dll"));
		}
	}
}
