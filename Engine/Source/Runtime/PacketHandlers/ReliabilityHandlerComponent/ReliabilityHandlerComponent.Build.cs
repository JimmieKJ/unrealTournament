// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ReliabilityHandlerComponent : ModuleRules
{
    public ReliabilityHandlerComponent(TargetInfo Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "PacketHandler",
            }
        );
    }
}
