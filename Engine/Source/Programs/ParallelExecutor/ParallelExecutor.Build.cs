// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ParallelExecutor : ModuleRules
{
	public ParallelExecutor(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");
		PublicIncludePaths.Add("Runtime/Launch/Private"); // Yuck. Required for RequiredProgramMainCPPInclude.h. (Also yuck).

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Projects", "XmlParser" });
	}
}
