// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTTPChunkInstaller : ModuleRules
{
	public HTTPChunkInstaller(TargetInfo Target)
	{
		PrivateDependencyModuleNames.Add("Core");
        PrivateDependencyModuleNames.Add("Engine");
        PrivateDependencyModuleNames.Add("PakFile");
		PrivateDependencyModuleNames.Add("HTTP");
        PrivateDependencyModuleNames.Add("BuildPatchServices");

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}