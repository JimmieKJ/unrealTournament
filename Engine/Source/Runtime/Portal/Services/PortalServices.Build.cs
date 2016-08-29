// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PortalServices : ModuleRules
	{
		public PortalServices(TargetInfo Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);

            Definitions.Add("WITH_PORTAL_SERVICES=1");
		}
	}
}
