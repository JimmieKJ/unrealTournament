// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AndroidMedia : ModuleRules
	{
        public AndroidMedia(TargetInfo Target)
		{
            DynamicallyLoadedModuleNames.AddRange(
                new string[] {
                    "Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				    "Engine",
                    "RenderCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
                    "Media",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AndroidMedia/Private",
					"AndroidMedia/Private/Player",
				});
		}
	}
}
