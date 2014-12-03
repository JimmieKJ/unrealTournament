// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealTournament : ModuleRules
{
    public UnrealTournament(TargetInfo Target)
    {
        bFasterWithoutUnity = true;
        MinFilesUsingPrecompiledHeaderOverride = 1;

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"Analytics",
				"AnalyticsET",
			}
		);
        
        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            // Hack for LINUXMessageBoxExt
            PublicDependencyModuleNames.AddRange(new string[] { "SDL2" });
        }

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "OnlineSubsystem", "OnlineSubsystemUtils", "RenderCore", "Navmesh", "Json", "HTTP" });
        if (Target.Type != TargetRules.TargetType.Server)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "AppFramework", "RHI", "Slate", "SlateCore", "SlateRHIRenderer" });
        }
        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "UnrealEd", "PropertyEditor" });
        }
    }
}
