// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealCodeAnalyzer : ModuleRules
{
	public UnrealCodeAnalyzer(TargetInfo Target)
	{
		bEnableExceptions = false;

		AddThirdPartyPrivateStaticDependencies(Target, "llvm");

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"Projects"
		});

		PublicIncludePaths.AddRange(new string[] {
			"Runtime/Launch/Public",
			"ThirdParty/llvm/3.5.0/include"
		});

		PrivateIncludePaths.AddRange(new string[] {
			"Runtime/Launch/Private",	// For LaunchEngineLoop.cpp include
			"Runtime/Launch/Public",
			"Programs/UnrealCodeAnalyzer/Public",
			"Programs/UnrealCodeAnalyzer/Private",
			"ThirdParty/llvm/3.5.0/include"
		});

		SharedPCHHeaderFile = "Programs/UnrealCodeAnalyzer/Public/UnrealCodeAnalyzerPCH.h";
	}
}
