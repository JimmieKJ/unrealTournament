// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AndroidMediaFactory : ModuleRules
	{
		public AndroidMediaFactory(TargetInfo Target)
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
					"AndroidMedia",
                    "Media",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"AndroidMediaFactory/Private",
				}
			);

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				DynamicallyLoadedModuleNames.Add("AndroidMedia");
			}
		}
	}
}
