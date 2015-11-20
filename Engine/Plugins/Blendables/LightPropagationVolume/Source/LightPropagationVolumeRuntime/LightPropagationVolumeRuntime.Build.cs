// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LightPropagationVolumeRuntime : ModuleRules
{
	public LightPropagationVolumeRuntime(TargetInfo Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {	
				"Core",
				"CoreUObject",
				"Engine",           // FBlendableManager
//				"RHI",
//				"RenderCore",
//				"ShaderCore",
			}
		);
	}
}
