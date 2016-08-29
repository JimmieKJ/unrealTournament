// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BlueprintRuntime : ModuleRules
	{
        public BlueprintRuntime(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/BlueprintRuntime/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				    "CoreUObject",
				}
			);
		}
	}
}