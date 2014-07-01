// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealTournamentEditor : ModuleRules
{
    public UnrealTournamentEditor(TargetInfo Target)
	{
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UnrealEd", "Slate", "SlateCore", "UnrealTournament" });
	}
}
