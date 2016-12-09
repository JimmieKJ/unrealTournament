// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PhysXVehicles : ModuleRules
	{
        public PhysXVehicles(TargetInfo Target)
		{
            PublicIncludePaths.Add("PhysXVehicles/Public");
            PrivateIncludePaths.Add("PhysXVehicles/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "ShaderCore",
                    "AnimGraphRuntime",
                    "RHI",
                    "PhysXVehicleLib"
				}
				);

            SetupModulePhysXAPEXSupport(Target);
        }
    }
}
