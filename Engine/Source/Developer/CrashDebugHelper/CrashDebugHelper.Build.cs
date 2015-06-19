// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CrashDebugHelper : ModuleRules
{
	public CrashDebugHelper( TargetInfo Target )
	{
		PrivateIncludePaths.AddRange(
		new string[] {
				"Developer/CrashDebugHelper/Private/",
				"Developer/CrashDebugHelper/Private/Linux",
				"Developer/CrashDebugHelper/Private/Mac",
				"Developer/CrashDebugHelper/Private/Windows",
			}
		);
		PrivateIncludePaths.Add( "ThirdParty/PLCrashReporter/plcrashreporter-master-5ae3b0a/Source" );

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"SourceControl"
			}
		);
	}
}
