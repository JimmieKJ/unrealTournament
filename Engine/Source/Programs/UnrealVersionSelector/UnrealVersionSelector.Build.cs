// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealVersionSelector : ModuleRules
{
	public UnrealVersionSelector(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");
		PublicIncludePaths.Add("Runtime/Launch/Private"); // Yuck. Required for RequiredProgramMainCPPInclude.h. (Also yuck).

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Projects", "DesktopPlatform" });
	}
}
