// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealAudioXAudio2 : ModuleRules
{
	public UnrealAudioXAudio2(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/UnrealAudio/Private");
		PublicIncludePaths.Add("Runtime/UnrealAudio/Public");

		PrivateIncludePathModuleNames.Add("TargetPlatform");

		PrivateDependencyModuleNames.AddRange(
		new string[] {
				"Core",
				"CoreUObject",
			}
		);
		
		PublicDependencyModuleNames.Add("UnrealAudio");

		AddEngineThirdPartyPrivateStaticDependencies(Target, 
			"DX11Audio"
		);

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
