// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SessionMessages : ModuleRules
	{
		public SessionMessages(TargetInfo Target)
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
					"Runtime/SessionMessages/Private",
				}
			);
		}
	}
}
