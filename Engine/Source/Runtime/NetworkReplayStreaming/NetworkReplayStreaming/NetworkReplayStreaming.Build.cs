// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NetworkReplayStreaming : ModuleRules
	{
		public NetworkReplayStreaming( TargetInfo Target )
		{
			PrivateIncludePaths.Add( "Runtime/NetworkReplayStreaming/NetworkReplayStreaming/Private" );

            PrivateIncludePathModuleNames.Add("OnlineSubsystem");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "Json",
                    // ... add other public dependencies that you statically link with here ...
				}
			);
		}
	}
}