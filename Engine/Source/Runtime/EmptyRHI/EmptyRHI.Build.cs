// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EmptyRHI : ModuleRules
{	
	public EmptyRHI(TargetInfo Target)
	{
//		BinariesSubFolder = "Empty";

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore",
				"ShaderCore"
			}
			);

		PrecompileForTargets = PrecompileTargetsType.None;
	}
}
