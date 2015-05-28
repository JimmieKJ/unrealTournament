// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class PLCrashReporter : ModuleRules
{
	public PLCrashReporter(TargetInfo Target)
	{
		Type = ModuleType.External;

		string PLCrashReporterPath = UEBuildConfiguration.UEThirdPartySourceDirectory + "PLCrashReporter/plcrashreporter-master-5ae3b0a/";

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicSystemIncludePaths.Add(PLCrashReporterPath + "Source");
			if (Target.Configuration == UnrealTargetConfiguration.Debug && BuildConfiguration.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(PLCrashReporterPath + "Mac/Debug/libCrashReporter-MacOSX-Static.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(PLCrashReporterPath + "Mac/Release/libCrashReporter-MacOSX-Static.a");
			}
		}
	}
}
