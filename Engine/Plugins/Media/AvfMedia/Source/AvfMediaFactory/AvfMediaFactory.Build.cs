// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AvfMediaFactory : ModuleRules
	{
		public AvfMediaFactory(TargetInfo Target)
		{
            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                    "Media",
				}
            );

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
                    "CoreUObject",
                    "MediaAssets",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"AvfMedia",
                    "Media",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"AvfMediaFactory/Private",
				}
			);

			if ((Target.Platform == UnrealTargetPlatform.IOS) || (Target.Platform == UnrealTargetPlatform.Mac))
			{
				DynamicallyLoadedModuleNames.Add("AvfMedia");
			}
		}
	}
}
