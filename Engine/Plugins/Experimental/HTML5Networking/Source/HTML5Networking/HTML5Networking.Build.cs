// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class HTML5Networking  : ModuleRules
	{
		public HTML5Networking(TargetInfo Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[] { 
                    "Core", 
                    "CoreUObject",
                    "Engine",
                    "ImageCore",
                    "Sockets",
                    "OnlineSubSystemUtils",
                    "WebSockets"
                }
            );
		}
	}
}
