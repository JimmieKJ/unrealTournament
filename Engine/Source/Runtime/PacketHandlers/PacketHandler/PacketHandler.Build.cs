// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PacketHandler : ModuleRules
{
    public PacketHandler(TargetInfo Target)
    {
		PrivateIncludePaths.Add("PacketHandler/Private");

        PublicDependencyModuleNames.AddRange
		(
            new string[]
			{
				"Core",
				"CoreUObject",
                "ReliabilityHandlerComponent",
            }
        );

        CircularlyReferencedDependentModules.Add("ReliabilityHandlerComponent");
    }
}
