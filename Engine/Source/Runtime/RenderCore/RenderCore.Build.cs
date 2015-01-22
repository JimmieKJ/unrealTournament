// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RenderCore : ModuleRules
{
	public RenderCore(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "RHI" });
	}
}
