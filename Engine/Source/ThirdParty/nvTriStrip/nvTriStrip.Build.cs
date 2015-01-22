// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class nvTriStrip : ModuleRules
{
	public nvTriStrip(TargetInfo Target)
	{
		Type = ModuleType.External;

		string NvTriStripPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "nvTriStrip/nvTriStrip-1.0.0/";
        PublicIncludePaths.Add(NvTriStripPath + "Inc");

		string NvTriStripLibPath = NvTriStripPath + "Lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			NvTriStripLibPath += "Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(NvTriStripLibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("nvTriStripD_64.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("nvTriStrip_64.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			NvTriStripLibPath += "Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(NvTriStripLibPath);
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("nvTriStripD.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("nvTriStrip.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
			PublicAdditionalLibraries.Add(NvTriStripLibPath + "Mac/libnvtristrip" + Postfix + ".a");
		}
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(NvTriStripLibPath + "Linux/" + Target.Architecture + "/libnvtristrip" + Postfix + ".a");
        }
	}
}
