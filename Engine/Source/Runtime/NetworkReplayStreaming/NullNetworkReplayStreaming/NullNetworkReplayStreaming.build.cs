// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NullNetworkReplayStreaming : ModuleRules
	{
		public NullNetworkReplayStreaming( TargetInfo Target )
		{
			PrivateIncludePaths.Add( "Runtime/NetworkReplayStreaming/NullNetworkReplayStreaming/Private" );

            PrivateIncludePathModuleNames.Add("OnlineSubsystem");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"NetworkReplayStreaming",
                    "Json"
				}
			);
		}
	}
}