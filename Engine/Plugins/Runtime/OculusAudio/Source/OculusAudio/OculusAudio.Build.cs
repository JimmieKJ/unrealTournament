// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OculusAudio : ModuleRules
	{
		public OculusAudio(TargetInfo Target)
		{
            PrivateIncludePathModuleNames.AddRange(
                new string[]
				{
					"TargetPlatform",
					"XAudio2"
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"OculusAudio/Private",
 					// ... add other private include paths required here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"XAudio2"
				}
				);

            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
				PrivateDependencyModuleNames.AddRange(new string[] { "LibOVRAudio" });
				RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/Oculus/Audio/Win64/ovraudio64.dll"));
            }

            AddThirdPartyPrivateStaticDependencies(Target, "DX11Audio");
		}
	}
}