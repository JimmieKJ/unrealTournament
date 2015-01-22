// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

using System;
using System.IO;

public class HTML5TargetPlatform : ModuleRules
{
	public HTML5TargetPlatform(TargetInfo Target)
	{
		BinariesSubFolder = "HTML5";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Sockets",
				"TargetPlatform",
				"DesktopPlatform",
			}
		);

		if (UEBuildConfiguration.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Engine");
//			PrivateIncludePathModuleNames.Add("TextureCompressor");
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Settings",
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"Developer/HTML5TargetPlatform/Classes"
			}
		);
	}
}
