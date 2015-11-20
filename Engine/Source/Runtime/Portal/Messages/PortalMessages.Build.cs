// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PortalMessages : ModuleRules
	{
		public PortalMessages(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "MessagingRpc",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Runtime/Portal/Messages/Private",
				}
			);
		}
	}
}
