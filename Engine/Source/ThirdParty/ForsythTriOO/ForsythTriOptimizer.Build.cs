// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ForsythTriOptimizer : ModuleRules
{
	public ForsythTriOptimizer(TargetInfo Target)
	{
		Type = ModuleType.External;

		string ForsythTriOptimizerPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "ForsythTriOO/";
        PublicIncludePaths.Add(ForsythTriOptimizerPath + "Src");

		string ForsythTriOptimizerLibPath = ForsythTriOptimizerPath + "Lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            ForsythTriOptimizerLibPath += "Win64/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(ForsythTriOptimizerLibPath);

			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("ForsythTriOptimizerD_64.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("ForsythTriOptimizer_64.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
            ForsythTriOptimizerLibPath += "Win32/VS" + WindowsPlatform.GetVisualStudioCompilerVersionName();
			PublicLibraryPaths.Add(ForsythTriOptimizerLibPath);
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add("ForsythTriOptimizerD.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add("ForsythTriOptimizer.lib");
			}
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(ForsythTriOptimizerLibPath + "Mac/libForsythTriOptimizer" + Postfix + ".a");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(ForsythTriOptimizerLibPath + "Linux/" + Target.Architecture + "/libForsythTriOptimizer" + Postfix + ".a");
        }
	}
}
