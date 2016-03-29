// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RSAEncryptionHandlerComponent : ModuleRules
{
    public RSAEncryptionHandlerComponent(TargetInfo Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "PacketHandler"
            }
        );

        if ((Target.Platform == UnrealTargetPlatform.Win64) ||
            (Target.Platform == UnrealTargetPlatform.Win32))
        {
            AddEngineThirdPartyPrivateStaticDependencies(Target,
                "CryptoPP"
                );
        }

        PublicIncludePathModuleNames.Add("CryptoPP");

		PrecompileForTargets = PrecompileTargetsType.None;
    }
}
