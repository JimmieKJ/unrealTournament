// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AvfMedia : ModuleRules
	{
		public AvfMedia(TargetInfo Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"RenderCore",
					"RHI",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"AvfMedia/Private",
					"AvfMedia/Private/Player",
					"AvfMedia/Private/Tracks",
				}
			);

			PublicFrameworks.AddRange(
				new string[] {
					"CoreMedia",
					"AVFoundation",
					"AudioToolbox",
					"QuartzCore"
				}
			);
		}
	}
}
