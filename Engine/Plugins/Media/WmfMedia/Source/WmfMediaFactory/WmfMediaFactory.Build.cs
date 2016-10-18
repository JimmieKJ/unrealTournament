// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WmfMediaFactory : ModuleRules
	{
		public WmfMediaFactory(TargetInfo Target)
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
					"Media",
					"WmfMedia",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"WmfMediaFactory/Private",
				}
			);

			if ((Target.Platform == UnrealTargetPlatform.Win32) || (Target.Platform == UnrealTargetPlatform.Win64))
			{
				DynamicallyLoadedModuleNames.Add("WmfMedia");
			}
		}
	}
}
