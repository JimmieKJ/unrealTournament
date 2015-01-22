// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameLiveStreaming : ModuleRules
	{
		public GameLiveStreaming( TargetInfo Target )
		{
			PublicDependencyModuleNames.AddRange(
				new string[] 
				{
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] 
				{
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"RenderCore",
					"ShaderCore",
					"RHI",
					"OnlineSubsystemUtils"	// @todo livestream: We need to move the useful features into engine and eliminate this module
				}
			);
		}
	}
}
