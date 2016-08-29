// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Icmp : ModuleRules
{
	public Icmp(TargetInfo Target)
	{
		Definitions.Add("ICMP_PACKAGE=1");

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Online/Icmp/Private",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] { 
				"Core", 
				"CoreUObject",
                "Sockets",
			}
		);
	}
}