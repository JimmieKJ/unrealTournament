// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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
                    "PortalServices",
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
