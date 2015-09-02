// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealTournamentEditor : ModuleRules
{
    public UnrealTournamentEditor(TargetInfo Target)
	{
        bFasterWithoutUnity = true;
        MinFilesUsingPrecompiledHeaderOverride = 1;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "UnrealEd", 
            "Slate", 
            "SlateCore", 
            "SlateRHIRenderer", 
            "UnrealTournament", 
            "OnlineSubsystem", 
            "OnlineSubsystemUtils", 
            "PakFile", 
            "StreamingFile", 
            "NetworkFile", 
            "UMG" });
	}
}
