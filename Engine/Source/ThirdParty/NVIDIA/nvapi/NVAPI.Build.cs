// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class NVAPI : ModuleRules
{
    public NVAPI(TargetInfo Target)
	{
		Type = ModuleType.External;

        string nvApiPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "NVIDIA/nvapi/";
        PublicSystemIncludePaths.Add(nvApiPath);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            string nvApiLibPath = nvApiPath + "amd64/";
            PublicLibraryPaths.Add(nvApiLibPath);
            PublicAdditionalLibraries.Add("nvapi64.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
            string nvApiLibPath = nvApiPath + "x86/";
            PublicLibraryPaths.Add(nvApiLibPath);
            PublicAdditionalLibraries.Add("nvapi.lib");
		}

	}
}

