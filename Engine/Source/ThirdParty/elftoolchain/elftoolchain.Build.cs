// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class elftoolchain : ModuleRules
{
	public elftoolchain(TargetInfo Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(UEBuildConfiguration.UEThirdPartySourceDirectory + "elftoolchain/include/" + Target.Architecture);

        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
			string LibDir = UEBuildConfiguration.UEThirdPartySourceDirectory + "elftoolchain/lib/Linux/" + Target.Architecture;
			PublicAdditionalLibraries.Add(LibDir + "/libelf.a");
			PublicAdditionalLibraries.Add(LibDir + "/libdwarf.a");
        }
	}
}
