// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Vulkan : ModuleRules
{
	public Vulkan(TargetInfo Target)
	{
		Type = ModuleType.External;
		
		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			string RootPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "Vulkan/Windows";
			string LibPath = RootPath + "/Bin";
			if (Target.Platform == UnrealTargetPlatform.Win32)
			{
				LibPath += "32";
			}

			PublicLibraryPaths.Add(LibPath);
			PublicAdditionalLibraries.Add("vulkan-1.lib");
			PublicAdditionalLibraries.Add("VKstatic.1.lib");

			PublicSystemIncludePaths.Add(RootPath + "/Include");
		}
	}
}
