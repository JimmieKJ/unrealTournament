// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EngineSettings : ModuleRules
	{
		public EngineSettings(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/EngineSettings/Private",
				}
			);
		}
	}
}
