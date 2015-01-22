// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BootstrapPackagedGame : ModuleRules
{
	public BootstrapPackagedGame(TargetInfo Target)
	{
		PublicAdditionalLibraries.Add("shlwapi.lib");
	}
}
