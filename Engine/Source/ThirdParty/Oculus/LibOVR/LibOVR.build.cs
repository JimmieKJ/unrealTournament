// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LibOVR : ModuleRules
{
	// Version of LibOVR
	public const int LibOVR_Major = 0;
	public       int LibOVR_Minor = 6;
	public const int LibOVR_Patch = 0;

	public LibOVR(TargetInfo Target)
	{
		Type = ModuleType.External;

        // PC uses SDK 0.6.0, while Mac uses 0.5.0
        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            LibOVR_Minor = 5;
            PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/LibOVR/" +
                                   "/LibOVR_" + LibOVR_Major + "_" +
                                   LibOVR_Minor + "_" + LibOVR_Patch);
        }
        else
        {
            PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/LibOVR/" +
                                   "/LibOVR_" + LibOVR_Major + "_" +
                                   LibOVR_Minor + "_" + LibOVR_Patch + "/Include");
            PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "Oculus/LibOVR/" +
                                   "/LibOVR_" + LibOVR_Major + "_" +
                                   LibOVR_Minor + "_" + LibOVR_Patch + "/Src");
        }
	}
}
