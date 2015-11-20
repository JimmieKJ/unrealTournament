// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D12RHI : ModuleRules
{
	public D3D12RHI(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/Windows/D3D12RHI/Private");

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

        AddThirdPartyPrivateStaticDependencies(Target, "DX12");
		AddThirdPartyPrivateStaticDependencies(Target, "DX11");
        AddThirdPartyPrivateStaticDependencies(Target, "NVAPI");
        AddThirdPartyPrivateStaticDependencies(Target, "AMD");
        
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

        Definitions.Add("SUB_ALLOCATED_DEFAULT_ALLOCATIONS=1");

		// Not fully implemented yet.
        Definitions.Add("SUPPORTS_MEMORY_RESIDENCY=0");
	}
}
