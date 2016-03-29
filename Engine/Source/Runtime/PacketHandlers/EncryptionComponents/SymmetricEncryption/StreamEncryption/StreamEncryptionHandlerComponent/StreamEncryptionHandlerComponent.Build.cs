// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class StreamEncryptionHandlerComponent : ModuleRules
{
    public StreamEncryptionHandlerComponent(TargetInfo Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
                "PacketHandler",
                "XORStreamEncryptor"
            }
        );

        CircularlyReferencedDependentModules.Add("XORStreamEncryptor");

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
