// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealTournament : ModuleRules
{
    bool IsLicenseeBuild()
    {
        return !Directory.Exists("Runtime/NotForLicensees");
    }

    public UnrealTournament(TargetInfo Target)
    {
        bFasterWithoutUnity = true;
        MinFilesUsingPrecompiledHeaderOverride = 1;

		PrivateIncludePaths.AddRange(new string[] {
			"UnrealTournament/Private/Slate",	
			"UnrealTournament/Private/Slate/Base",	
			"UnrealTournament/Private/Slate/Dialogs",	
			"UnrealTournament/Private/Slate/Menus",	
			"UnrealTournament/Private/Slate/Panels",	
			"UnrealTournament/Private/Slate/Toasts",	
			"UnrealTournament/Private/Slate/Widgets",	
			"UnrealTournament/Private/Slate/UIWindows",	
		});


		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"UTReplayStreamer",
			}
		);
        
        PublicDependencyModuleNames.AddRange(new string[] { 
                                                    "Core", 
                                                    "CoreUObject",
                                                    "Engine", 
                                                    "InputCore",
                                                    "GameplayTasks",
                                                    "AIModule", 
                                                    "OnlineSubsystem", 
                                                    "OnlineSubsystemUtils", 
                                                    "RenderCore", 
                                                    "Navmesh", 
                                                    "WebBrowser", 
                                                    "NetworkReplayStreaming",
                                                    "InMemoryNetworkReplayStreaming",
                                                    "Json", 
													"JsonUtilities",
                                                    "HTTP", 
                                                    "UMG", 
				                                    "Party",
				                                    "Lobby",
				                                    "Qos",
                                                    "BlueprintContext",
                                                    "EngineSettings", 
			                                        "Landscape",
                                                    "Foliage",
													"PerfCounters",
                                                    "PakFile",
													"UnrealTournamentFullScreenMovie"
                                                    });

        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "FriendsAndChat", "Sockets", "Analytics", "AnalyticsET" });
        if (Target.Type != TargetRules.TargetType.Server)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "AppFramework", "RHI", "SlateRHIRenderer", "MoviePlayer" });
        }
        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "UnrealEd", "SourceControl", "Matinee", "PropertyEditor", "ShaderCore" });
        }

        CircularlyReferencedDependentModules.Add("BlueprintContext");

        if (!IsLicenseeBuild())
        {            
            Definitions.Add("WITH_PROFILE=1");
            Definitions.Add("WITH_SOCIAL=1");

            PublicIncludePathModuleNames.Add("Social");

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "OnlineSubsystemMcp",
                    "McpProfileSys",
                    "UTMcpProfile",
                    "Social",
                }
            );
        }
        else
        {
            Definitions.Add("WITH_PROFILE=0");
        }
    }
}
