// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SSF : ModuleRules
{
    public SSF(TargetInfo Target)
	{
		Type = ModuleType.External;

		string SSFDirectory = UEBuildConfiguration.UEThirdPartySourceDirectory + "SSF/";
        string SSFLibPath = SSFDirectory;
        PublicIncludePaths.Add(SSFDirectory + "Public");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            SSFLibPath = SSFLibPath + "lib/win64/vs" + WindowsPlatform.GetVisualStudioCompilerVersionName() + "/";
            PublicLibraryPaths.Add(SSFLibPath);

            if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
            {
                PublicAdditionalLibraries.Add("SSFd.lib");
            }
            else
            {
                PublicAdditionalLibraries.Add("SSF.lib");
            }
		}
	}
}

