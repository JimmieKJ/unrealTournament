// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class WmfMediaEditor : ModuleRules
	{
		public WmfMediaEditor(TargetInfo Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaAssets",
					"UnrealEd",
                }
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"WmfMediaEditor/Private",
				}
			);
		}
	}
}
