// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
        PrivateDependencyModuleNames.Add("OnlineSubsystem");
        PrivateDependencyModuleNames.Add("OnlineSubsystemUtils");
	}
}