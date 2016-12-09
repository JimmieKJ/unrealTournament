// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Stomp : ModuleRules
{
    public Stomp(TargetInfo Target)
    {
        Definitions.Add("STOMP_PACKAGE=1");

		bool bShouldUseModule = 
			Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64 ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.Platform == UnrealTargetPlatform.Linux;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core"
			}
		);

		if (bShouldUseModule)
		{
			Definitions.Add("WITH_STOMP=1");

			PrivateIncludePaths.AddRange(
				new string[]
				{
				"Runtime/Online/Stomp/Private",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"WebSockets"
				}
			);
		}
	}
}
