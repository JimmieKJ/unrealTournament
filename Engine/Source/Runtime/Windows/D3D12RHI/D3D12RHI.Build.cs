// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		Definitions.Add("SUB_ALLOCATED_DEFAULT_ALLOCATIONS=1");
		Definitions.Add("DEBUG_RESOURCE_STATES=0");

		Definitions.Add("ENABLE_RESIDENCY_MANAGEMENT=1");
		// How many residency packets can be in flight before the rendering thread
		// blocks for them to drain. Should be ~ NumBufferedFrames * AvgNumSubmissionsPerFrame i.e.
		// enough to ensure that the GPU is rarely blocked by residency work
		 Definitions.Add("RESIDENCY_PIPELINE_DEPTH=6");

		// DX11 doesn't support higher MSAA count
		Definitions.Add("DX_MAX_MSAA_COUNT=8");

		// This is a value that should be tweaked to fit the app, lower numbers will have better performance
		Definitions.Add("MAX_SRVS=22");
		Definitions.Add("MAX_CBS=8");

		Definitions.Add("ASYNC_DEFERRED_DELETION=1");
	}
}
