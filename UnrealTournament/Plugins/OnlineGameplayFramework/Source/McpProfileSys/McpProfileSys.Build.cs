// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class McpProfileSys : ModuleRules
{
	public McpProfileSys(TargetInfo Target)
    {
        Definitions.Add("MCPPROFILESYS_PACKAGE=1");

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
			}
		);

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Engine",
					"OnlineSubsystemUtils"
				}
			);
		}
    }
}
