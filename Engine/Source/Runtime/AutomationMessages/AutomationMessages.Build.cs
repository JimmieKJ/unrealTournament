// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AutomationMessages : ModuleRules
	{
		public AutomationMessages(TargetInfo Target)
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
					"Editor/AutomationMessages/Private",
				}
			);
		}
	}
}
