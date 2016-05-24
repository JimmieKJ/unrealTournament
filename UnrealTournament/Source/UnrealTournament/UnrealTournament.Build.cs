// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealTournament : ModuleRules
{
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
				"Analytics",
				"AnalyticsET",
				"UTReplayStreamer",
			}
		);
        
        PublicDependencyModuleNames.AddRange(new string[] { 
                                                    "Core", 
                                                    "CoreUObject", 
                                                    "Engine", 
                                                    "InputCore", 
                                                    "AIModule", 
                                                    "OnlineSubsystem", 
                                                    "OnlineSubsystemUtils", 
                                                    "RenderCore", 
                                                    "Navmesh", 
                                                    "WebBrowser", 
                                                    "NetworkReplayStreaming", 
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
                                                    });

        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "Sockets" });
        if (Target.Type != TargetRules.TargetType.Server)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "AppFramework", "RHI", "SlateRHIRenderer", "MoviePlayer" });
        }
        if (Target.Type == TargetRules.TargetType.Editor)
        {
            PublicDependencyModuleNames.AddRange(new string[] { "UnrealEd", "SourceControl", "PropertyEditor", "ShaderCore" });
        }

        CircularlyReferencedDependentModules.Add("BlueprintContext");
        
        if (UEBuildConfiguration.bCompileMcpOSS == true)
        {
            // bCompileMcpOSS has become a dumping ground for the detection of external builders, this should get formalized into a real concept

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PublicDependencyModuleNames.AddRange(new string[] { "WinDualShock" });
            }

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
