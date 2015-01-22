// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SDL : ModuleRules
{
	public SDL(TargetInfo Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.HTML5 && Target.Architecture == "-win32")
		{
			PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "SDL");
			PublicAdditionalLibraries.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "SDL/lib/x86/SDL.lib");
			PublicDelayLoadDLLs.Add("SDL.dll");
		}
	}
}
