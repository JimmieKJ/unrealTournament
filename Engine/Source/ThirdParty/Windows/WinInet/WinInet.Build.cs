// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WinInet : ModuleRules
{
	public WinInet(TargetInfo Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win32 ||
			Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add("wininet.lib");
		}
	}
}

