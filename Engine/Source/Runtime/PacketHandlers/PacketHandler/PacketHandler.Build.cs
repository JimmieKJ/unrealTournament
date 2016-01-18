// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PacketHandler : ModuleRules
{
    public PacketHandler(TargetInfo Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "ReliabilityHandlerComponent",
            }
        );

        CircularlyReferencedDependentModules.Add("ReliabilityHandlerComponent");
    }
}
