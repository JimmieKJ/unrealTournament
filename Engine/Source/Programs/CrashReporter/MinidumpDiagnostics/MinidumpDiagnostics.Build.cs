// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MinidumpDiagnostics : ModuleRules
{
	public MinidumpDiagnostics( TargetInfo Target )
	{
		PrivateIncludePathModuleNames.Add( "Launch" );
		PrivateIncludePaths.Add( "Runtime/Launch/Private" );
	
		PrivateDependencyModuleNames.AddRange(
			new string[] 
			{ 
				"Core",
				"CrashDebugHelper",
				"PerforceSourceControl",
				"Projects"
			}
			);
	}
}
