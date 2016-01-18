// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LootTables : ModuleRules
{
	public LootTables(TargetInfo Target)
    {
        Definitions.Add("LOOTTABLES_PACKAGE=1");

        PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core",
				"CoreUObject",
				"Json",
				"JsonUtilities",
				"Engine",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"ExcelBridgeEditor"
			}
		);
    }
}
