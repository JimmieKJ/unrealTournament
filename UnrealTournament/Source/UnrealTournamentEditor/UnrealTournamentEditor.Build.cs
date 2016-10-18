// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealTournamentEditor : ModuleRules
{
    bool IsLicenseeBuild()
    {
        return !Directory.Exists("Runtime/NotForLicensees");
    }

    public UnrealTournamentEditor(TargetInfo Target)
	{
        bFasterWithoutUnity = true;
        MinFilesUsingPrecompiledHeaderOverride = 1;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "UnrealEd", 
            "Matinee",
            "Slate", 
            "SlateCore", 
            "SlateRHIRenderer", 
            "UnrealTournament", 
            "OnlineSubsystem", 
            "OnlineSubsystemUtils",
            "BlueprintContext",
            "BlueprintContextEditor",
            "PakFile", 
            "StreamingFile", 
            "NetworkFile", 
			"PerfCounters",
			"UMGEditor",
            "UMG" });

        if (!IsLicenseeBuild())
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
			        "McpProfileSys",
			        "GameSubCatalog",
			        "GameSubCatalogEditor",
			        "LootTables",
                }
            );
        }
	}

}
