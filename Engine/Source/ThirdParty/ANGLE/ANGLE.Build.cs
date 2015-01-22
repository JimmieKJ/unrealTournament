// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class ANGLE : ModuleRules
{
	public ANGLE(TargetInfo Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
		{
			PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "ANGLE");
			PublicAdditionalLibraries.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "ANGLE/lib/libGLESv2.lib");
			PublicAdditionalLibraries.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "ANGLE/lib/libEGL.lib");

            PublicDelayLoadDLLs.AddRange(
                       new string[] {
						"libGLESv2.dll", 
    					"libEGL.dll" }
                       );

		}
	}
}
