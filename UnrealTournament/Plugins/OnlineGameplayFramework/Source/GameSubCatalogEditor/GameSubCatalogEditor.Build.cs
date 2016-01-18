// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameSubCatalogEditor : ModuleRules
{
	public GameSubCatalogEditor(TargetInfo Target)
    {
        Definitions.Add("GAMESUBCATALOGEDITOR_PACKAGE=1");

        PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"HTTP",
				"Json",
				"JsonUtilities",
				"Internationalization",
				"Engine",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"OnlineSubsystem",
				"OnlineSubsystemMcp",
				"McpProfileSys",
				"GameSubCatalog",
			}
		);
    }
}
