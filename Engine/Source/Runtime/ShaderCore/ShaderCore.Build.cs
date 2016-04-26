// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShaderCore : ModuleRules
{
	public ShaderCore(TargetInfo Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] { "RHI", "RenderCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Core" });

		PrivateIncludePathModuleNames.AddRange(new string[] { "DerivedDataCache", "TargetPlatform" });
	}
}