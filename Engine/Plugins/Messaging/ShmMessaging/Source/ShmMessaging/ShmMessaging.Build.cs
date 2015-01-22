// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ShmMessaging : ModuleRules
	{
		public ShmMessaging(TargetInfo Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Messaging",
					"Settings",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"ShmMessaging/Private",
					"ShmMessaging/Private/Allocator",
					"ShmMessaging/Private/Shared",
					"ShmMessaging/Private/Transport",
				}
			);
		}
	}
}
