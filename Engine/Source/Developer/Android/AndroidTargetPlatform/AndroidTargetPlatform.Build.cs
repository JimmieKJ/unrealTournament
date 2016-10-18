// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AndroidTargetPlatform : ModuleRules
{
	public AndroidTargetPlatform(TargetInfo Target)
	{
		BinariesSubFolder = "Android";

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"TargetPlatform",
                "DesktopPlatform",
				"AndroidDeviceDetection",
			}
		);

		PublicIncludePaths.AddRange(
			new string[]
			{
				"Runtime/Core/Public/Android"
			}
		);

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
			PrivateIncludePathModuleNames.Add("TextureCompressor");		//@todo android: AndroidTargetPlatform.Build
		}

        Definitions.Add("WITH_OGGVORBIS=1");

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Developer/AndroidTargetPlatform/Classes",
                "SessionServices"
            }
        );
	}
}