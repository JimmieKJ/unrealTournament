// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AndroidMedia : ModuleRules
	{
        public AndroidMedia(TargetInfo Target)
		{
            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                    "Media",
                    "Settings",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				    "CoreUObject",
				    "Engine",
                    "RenderCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
                    "Media",
					"Settings",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AndroidMedia/Private",
				});
		}
	}
}
