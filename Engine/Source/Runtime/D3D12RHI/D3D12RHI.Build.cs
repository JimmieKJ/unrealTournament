// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D12RHI : ModuleRules
{
	public D3D12RHI(TargetInfo Target)
	{
		PrivateIncludePaths.Add("Runtime/D3D12RHI/Private");

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

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		///////////////////////////////////////////////////////////////
		// Platform agnostic defines
		///////////////////////////////////////////////////////////////
		Definitions.Add("SUB_ALLOCATED_DEFAULT_ALLOCATIONS=1");

		Definitions.Add("DEBUG_RESOURCE_STATES=0");

		// DX12 doesn't support higher MSAA count
		Definitions.Add("DX_MAX_MSAA_COUNT=8");

		// This is a value that should be tweaked to fit the app, lower numbers will have better performance
		Definitions.Add("MAX_SRVS=32");
        Definitions.Add("MAX_SAMPLERS=16");
        Definitions.Add("MAX_UAVS=8");
        Definitions.Add("MAX_CBS=8");

		// This value controls how many root constant buffers can be used per shader stage in a root signature.
		// Note: Using root descriptors significantly increases the size of root signatures (each root descriptor is 2 DWORDs).
		Definitions.Add("MAX_ROOT_CBVS=MAX_CBS");

		// PC uses a multiple root signatures that are optimized for a particular draw/dispatch.
		Definitions.Add("USE_STATIC_ROOT_SIGNATURE=0");

		// How many residency packets can be in flight before the rendering thread
		// blocks for them to drain. Should be ~ NumBufferedFrames * AvgNumSubmissionsPerFrame i.e.
		// enough to ensure that the GPU is rarely blocked by residency work
		Definitions.Add("RESIDENCY_PIPELINE_DEPTH=6");

        ///////////////////////////////////////////////////////////////
        // Platform specific defines
        ///////////////////////////////////////////////////////////////

        if (Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.XboxOne)
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");

			Definitions.Add("ENABLE_RESIDENCY_MANAGEMENT=1");

			Definitions.Add("ASYNC_DEFERRED_DELETION=1");

			Definitions.Add("PLATFORM_SUPPORTS_MGPU=1");

			Definitions.Add("PIPELINE_STATE_FILE_LOCATION=FPaths::GameSavedDir()");
		}
		else if (Target.Platform == UnrealTargetPlatform.XboxOne)
		{
			Definitions.Add("ENABLE_RESIDENCY_MANAGEMENT=0");

			Definitions.Add("ASYNC_DEFERRED_DELETION=0");

			Definitions.Add("PLATFORM_SUPPORTS_MGPU=0");

			Definitions.Add("ASYNC_DEFERRED_DELETION=0");

			// The number of sampler descriptors with the maximum value of 2048
			// If the heap type is unbounded the number could be increased to avoid rollovers.
			Definitions.Add("ENABLE_UNBOUNDED_SAMPLER_DESCRIPTORS=0");
			Definitions.Add("NUM_SAMPLER_DESCRIPTORS=D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE");

			// Xbox doesn't have DXGI but common code needs this defined for headers
			Definitions.Add("DXGI_QUERY_VIDEO_MEMORY_INFO=int");

			Definitions.Add("PIPELINE_STATE_FILE_LOCATION=FPaths::GameContentDir()");

		}
	}
}
