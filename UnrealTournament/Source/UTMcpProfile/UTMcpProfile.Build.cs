// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UTMcpProfile : ModuleRules
    {
        public UTMcpProfile(TargetInfo Target)
        {
			PrivateDependencyModuleNames.AddRange( 
				new string[] 
				{ 
					"Core", 
					"CoreUObject", 
					"Engine", 
					"OnlineSubsystem", 
					"OnlineSubsystemUtils", 
					"OnlineSubsystemMcp", 
					"Json", 
					"HTTP", 
				} );
		}
    }
}
