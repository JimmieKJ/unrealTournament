// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class EncryptionHandlerComponent : ModuleRules
{
    public EncryptionHandlerComponent(TargetInfo Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "PacketHandler",
                "BlockEncryptionHandlerComponent",
                "StreamEncryptionHandlerComponent",
                "RSAEncryptionHandlerComponent",
            }
        );
    }
}
