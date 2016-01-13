// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UTReplayStreamer : ModuleRules
    {
        public UTReplayStreamer(TargetInfo Target)
        {
			PrivateIncludePaths.Add( "UTReplayStreamer/Private" );

			PrivateDependencyModuleNames.AddRange( 
				new string[] 
				{ 
					"Core", 
					"CoreUObject", 
					"Engine", 
					"OnlineSubsystem", 
					"OnlineSubsystemUtils", 
					"Json", 
					"HTTP", 
					"NetworkReplayStreaming", 
					"HttpNetworkReplayStreaming",
                    "NullNetworkReplayStreaming",
				} );
		}
    }
}
