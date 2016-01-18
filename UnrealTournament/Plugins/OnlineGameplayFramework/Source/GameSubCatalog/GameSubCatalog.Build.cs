// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameSubCatalog : ModuleRules
{
	public GameSubCatalog(TargetInfo Target)
    {
        Definitions.Add("GAMESUBCATALOG_PACKAGE=1");

        PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"HTTP",
				"Json",
				"JsonUtilities",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"OnlineSubsystem",
				"OnlineSubsystemMcp",
				"McpProfileSys",
			}
		);
    }
}
