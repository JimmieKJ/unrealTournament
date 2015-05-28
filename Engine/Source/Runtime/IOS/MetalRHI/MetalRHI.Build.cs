// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MetalRHI : ModuleRules
{	
	public MetalRHI(TargetInfo Target)
	{

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore",
				"ShaderCore",
				"UtilityShaders",
			}
			);
			
		PublicWeakFrameworks.Add("Metal");
	}
}
