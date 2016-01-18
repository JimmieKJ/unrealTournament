// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class BlockEncryptionHandlerComponent : ModuleRules
{
    public BlockEncryptionHandlerComponent(TargetInfo Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "PacketHandler",
                "XORBlockEncryptor",
            }
        );

        CircularlyReferencedDependentModules.Add("XORBlockEncryptor");

        if ((Target.Platform == UnrealTargetPlatform.Win64) ||
        (Target.Platform == UnrealTargetPlatform.Win32))
        {
            AddThirdPartyPrivateStaticDependencies(Target,
                "CryptoPP"
                );
        }

        PublicIncludePathModuleNames.Add("CryptoPP");

    }
}
