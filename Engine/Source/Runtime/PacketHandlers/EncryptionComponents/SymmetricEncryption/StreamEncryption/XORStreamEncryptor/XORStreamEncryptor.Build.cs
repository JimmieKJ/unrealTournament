// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class XORStreamEncryptor : ModuleRules
{
    public XORStreamEncryptor(TargetInfo Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "StreamEncryptionHandlerComponent"
            }
        );

		PrecompileForTargets = PrecompileTargetsType.None;
    }
}
