// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnalyticsSwrve : ModuleRules
	{
		public AnalyticsSwrve(TargetInfo Target)
		{
			PublicDependencyModuleNames.Add("Core");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Analytics",
					"HTTP",
					"Json",
				}
			);

			if(!UnrealBuildTool.IsDesktopPlatform(Target.Platform))
			{
				PrecompileForTargets = PrecompileTargetsType.None;
			}
		}
	}
}
