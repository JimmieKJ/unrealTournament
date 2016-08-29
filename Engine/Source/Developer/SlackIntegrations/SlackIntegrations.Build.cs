// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlackIntegrations : ModuleRules
{
    public SlackIntegrations(TargetInfo Target)
    {
		PublicIncludePaths.Add("Developer/SlackIntegrations/Public");

		PrivateIncludePaths.Add("Developer/SlackIntegrations/Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
            new string[] {
				"HTTP",
			}
		);
    }
}

