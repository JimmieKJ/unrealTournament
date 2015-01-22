// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealFileServer : ModuleRules
{
	public UnrealFileServer(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Runtime/Launch/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "Core",
                "DirectoryWatcher",
                "NetworkFileSystem",
                "SandboxFile",
				"Sockets",
				"Projects",
           }
		);
	}
}
