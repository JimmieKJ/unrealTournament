// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PerfCounters : ModuleRules
{
    public PerfCounters(TargetInfo Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"Json",
				"Sockets",
				"HTTP"
			}
        );
	}
}
