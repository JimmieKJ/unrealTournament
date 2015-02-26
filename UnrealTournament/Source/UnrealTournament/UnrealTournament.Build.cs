// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
        
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "OnlineSubsystem", "OnlineSubsystemUtils", "RenderCore", "Navmesh", "WebBrowser", "Json", "HTTP", "UMG", "EngineSettings" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "FriendsAndChat", "Sockets" });
        if (Target.Type != TargetRules.TargetType.Server)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "AppFramework", "RHI", "SlateRHIRenderer", "MoviePlayer" });
        }
        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "UnrealEd", "PropertyEditor" });
        }

        if (UEBuildConfiguration.bCompileMcpOSS == true)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "OnlineSubsystemMcp",
                }
            );
        }
    }
}
