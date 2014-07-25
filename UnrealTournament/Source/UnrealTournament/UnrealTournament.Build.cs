// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealTournament : ModuleRules
{
	public UnrealTournament(TargetInfo Target)
	{
        bFasterWithoutUnity = true;
        MinFilesUsingPrecompiledHeaderOverride = 1;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "RHI", "Slate", "SlateCore", "SlateRHIRenderer" });
        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "UnrealEd", "PropertyEditor" });
        }
	}
}
