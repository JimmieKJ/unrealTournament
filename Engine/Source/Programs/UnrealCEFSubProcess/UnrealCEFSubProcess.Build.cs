// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealCEFSubProcess : ModuleRules
{
	public UnrealCEFSubProcess(TargetInfo Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Programs/UnrealCEFSubProcess/Private",
				"Runtime/Launch/Private",					// for LaunchEngineLoop.cpp include
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Projects",
				"CEF3Utils",
			}
		);

		AddThirdPartyPrivateStaticDependencies(Target,
			"CEF3"
			);
	}
}

