// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ArchVisCharacter : ModuleRules
	{
		public ArchVisCharacter(TargetInfo Target)
		{
			PrivateIncludePaths.Add("ArchVisCharacter/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
//                     "RenderCore",
//                     "ShaderCore",
//                     "RHI"
				}
				);
		}
	}
}
