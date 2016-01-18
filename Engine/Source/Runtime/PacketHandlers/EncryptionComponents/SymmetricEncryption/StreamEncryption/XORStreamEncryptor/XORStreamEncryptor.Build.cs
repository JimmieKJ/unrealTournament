// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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
    }
}
