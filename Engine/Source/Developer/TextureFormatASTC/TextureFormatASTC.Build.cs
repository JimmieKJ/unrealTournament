// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatASTC : ModuleRules
{
	public TextureFormatASTC(TargetInfo Target)
	{
        PrivateIncludePathModuleNames.AddRange(new string[] { "TargetPlatform", "TextureCompressor", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "ImageCore", "ImageWrapper", "TextureFormatIntelISPCTexComp" });

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
            RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/ARM/Win32/astcenc.exe"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/ARM/Mac/astcenc"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			RuntimeDependencies.Add(new RuntimeDependency("$(EngineDir)/Binaries/ThirdParty/ARM/Linux32/astcenc"));
		}
	}
}