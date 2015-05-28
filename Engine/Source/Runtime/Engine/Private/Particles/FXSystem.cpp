// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FXSystem.cpp: Implementation of the effects system.
=============================================================================*/

#include "EnginePrivate.h"
#include "SystemSettings.h"
#include "RHI.h"
#include "RHIStaticStates.h"
#include "RenderResource.h"
#include "FXSystemPrivate.h"
#include "../VectorField.h"
#include "../GPUSort.h"
#include "ParticleCurveTexture.h"
#include "VectorField/VectorField.h"
#include "Components/VectorFieldComponent.h"

/*-----------------------------------------------------------------------------
	External FX system interface.
-----------------------------------------------------------------------------*/

FFXSystemInterface* FFXSystemInterface::Create(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform)
{
	return new FFXSystem(InFeatureLevel, InShaderPlatform);
}

void FFXSystemInterface::Destroy( FFXSystemInterface* FXSystem )
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FDestroyFXSystemCommand,
		FFXSystemInterface*, FXSystem, FXSystem,
	{
		delete FXSystem;
	});
}

FFXSystemInterface::~FFXSystemInterface()
{
}

/*------------------------------------------------------------------------------
	FX system console variables.
------------------------------------------------------------------------------*/

namespace FXConsoleVariables
{
	int32 VisualizeGPUSimulation = 0;
	int32 bAllowGPUSorting = true;
	int32 bAllowCulling = true;
	int32 bFreezeGPUSimulation = false;
	int32 bFreezeParticleSimulation = false;
	int32 bAllowAsyncTick = false;
	float ParticleSlackGPU = 0.02f;
	int32 MaxParticleTilePreAllocation = 100;
	int32 MaxCPUParticlesPerEmitter = 1000;
	int32 MaxGPUParticlesSpawnedPerFrame = 1024 * 1024;
	int32 GPUSpawnWarningThreshold = 20000;
	float GPUCollisionDepthBounds = 500.0f;
	TAutoConsoleVariable<int32> TestGPUSort(TEXT("FX.TestGPUSort"),0,TEXT("Test GPU sort. 1: Small, 2: Large, 3: Exhaustive, 4: Random"),ECVF_Cheat);

	/** Register references to flags. */
	FAutoConsoleVariableRef CVarVisualizeGPUSimulation(
		TEXT("FX.VisualizeGPUSimulation"),
		VisualizeGPUSimulation,
		TEXT("Visualize the current state of GPU simulation.\n")
		TEXT("0 = off\n")
		TEXT("1 = visualize particle state\n")
		TEXT("2 = visualize curve texture"),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowGPUSorting(
		TEXT("FX.AllowGPUSorting"),
		bAllowGPUSorting,
		TEXT("Allow particles to be sorted on the GPU."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarFreezeGPUSimulation(
		TEXT("FX.FreezeGPUSimulation"),
		bFreezeGPUSimulation,
		TEXT("Freeze particles simulated on the GPU."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarFreezeParticleSimulation(
		TEXT("FX.FreezeParticleSimulation"),
		bFreezeParticleSimulation,
		TEXT("Freeze particle simulation."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowAsyncTick(
		TEXT("FX.AllowAsyncTick"),
		bAllowAsyncTick,
		TEXT("allow parallel ticking of particle systems."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarParticleSlackGPU(
		TEXT("FX.ParticleSlackGPU"),
		ParticleSlackGPU,
		TEXT("Amount of slack to allocate for GPU particles to prevent tile churn as percentage of total particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarMaxParticleTilePreAllocation(
		TEXT("FX.MaxParticleTilePreAllocation"),
		MaxParticleTilePreAllocation,
		TEXT("Maximum tile preallocation for GPU particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarMaxCPUParticlesPerEmitter(
		TEXT("FX.MaxCPUParticlesPerEmitter"),
		MaxCPUParticlesPerEmitter,
		TEXT("Maximum number of CPU particles allowed per-emitter."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarMaxGPUParticlesSpawnedPerFrame(
		TEXT("FX.MaxGPUParticlesSpawnedPerFrame"),
		MaxGPUParticlesSpawnedPerFrame,
		TEXT("Maximum number of GPU particles allowed to spawn per-frame per-emitter."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarGPUSpawnWarningThreshold(
		TEXT("FX.GPUSpawnWarningThreshold"),
		GPUSpawnWarningThreshold,
		TEXT("Warning threshold for spawning of GPU particles."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarGPUCollisionDepthBounds(
		TEXT("FX.GPUCollisionDepthBounds"),
		GPUCollisionDepthBounds,
		TEXT("Limits the depth bounds when searching for a collision plane."),
		ECVF_Cheat
		);
	FAutoConsoleVariableRef CVarAllowCulling(
		TEXT("FX.AllowCulling"),
		bAllowCulling,
		TEXT("Allow emitters to be culled."),
		ECVF_Cheat
		);
}

/*------------------------------------------------------------------------------
	FX system.
------------------------------------------------------------------------------*/

FFXSystem::FFXSystem(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform)
	: ParticleSimulationResources(NULL)
	, FeatureLevel(InFeatureLevel)
	, ShaderPlatform(InShaderPlatform)
#if WITH_EDITOR
	, bSuspended(false)
#endif // #if WITH_EDITOR
{
	InitGPUSimulation();
}

FFXSystem::~FFXSystem()
{
	DestroyGPUSimulation();
}

void FFXSystem::Tick(float DeltaSeconds)
{
	if (RHISupportsGPUParticles(FeatureLevel))
	{
		// Test GPU sorting if requested.
		if (FXConsoleVariables::TestGPUSort.GetValueOnGameThread() != 0)
		{
			TestGPUSort((EGPUSortTest)FXConsoleVariables::TestGPUSort.GetValueOnGameThread(), GetFeatureLevel());
			// Reset CVar
			static IConsoleVariable* CVarTestGPUSort = IConsoleManager::Get().FindConsoleVariable(TEXT("FX.TestGPUSort"));

			// todo: bad use of console variables, this should be a console command 
			CVarTestGPUSort->Set(0, ECVF_SetByCode);
		}

		// Before ticking GPU particles, ensure any pending curves have been
		// uploaded.
		GParticleCurveTexture.SubmitPendingCurves();
	}
}

#if WITH_EDITOR
void FFXSystem::Suspend()
{
	if (!bSuspended && RHISupportsGPUParticles(FeatureLevel))
	{
		ReleaseGPUResources();
		bSuspended = true;
	}
}

void FFXSystem::Resume()
{
	if (bSuspended && RHISupportsGPUParticles(FeatureLevel))
	{
		bSuspended = false;
		InitGPUResources();
	}
}
#endif // #if WITH_EDITOR

/*------------------------------------------------------------------------------
	Vector field instances.
------------------------------------------------------------------------------*/

void FFXSystem::AddVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles(FeatureLevel))
	{
		check( VectorFieldComponent->VectorFieldInstance == NULL );
		check( VectorFieldComponent->FXSystem == this );

		if ( VectorFieldComponent->VectorField )
		{
			FVectorFieldInstance* Instance = new FVectorFieldInstance();
			VectorFieldComponent->VectorField->InitInstance(Instance, /*bPreviewInstance=*/ false);
			VectorFieldComponent->VectorFieldInstance = Instance;
			Instance->WorldBounds = VectorFieldComponent->Bounds.GetBox();
			Instance->Intensity = VectorFieldComponent->Intensity;
			Instance->Tightness = VectorFieldComponent->Tightness;

			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				FAddVectorFieldCommand,
				FFXSystem*, FXSystem, this,
				FVectorFieldInstance*, Instance, Instance,
				FMatrix, ComponentToWorld, VectorFieldComponent->ComponentToWorld.ToMatrixWithScale(),
			{
				Instance->UpdateTransforms( ComponentToWorld );
				Instance->Index = FXSystem->VectorFields.AddUninitialized().Index;
				FXSystem->VectorFields[ Instance->Index ] = Instance;
			});
		}
	}
}

void FFXSystem::RemoveVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles(FeatureLevel))
	{
		check( VectorFieldComponent->FXSystem == this );

		FVectorFieldInstance* Instance = VectorFieldComponent->VectorFieldInstance;
		VectorFieldComponent->VectorFieldInstance = NULL;

		if ( Instance )
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				FRemoveVectorFieldCommand,
				FFXSystem*, FXSystem, this,
				FVectorFieldInstance*, Instance, Instance,
			{
				if ( Instance->Index != INDEX_NONE )
				{
					FXSystem->VectorFields.RemoveAt( Instance->Index );
					delete Instance;
				}
			});
		}
	}
}

void FFXSystem::UpdateVectorField( UVectorFieldComponent* VectorFieldComponent )
{
	if (RHISupportsGPUParticles(FeatureLevel))
	{
		check( VectorFieldComponent->FXSystem == this );

		FVectorFieldInstance* Instance = VectorFieldComponent->VectorFieldInstance;

		if ( Instance )
		{
			struct FUpdateVectorFieldParams
			{
				FBox Bounds;
				FMatrix ComponentToWorld;
				float Intensity;
				float Tightness;
			};

			FUpdateVectorFieldParams UpdateParams;
			UpdateParams.Bounds = VectorFieldComponent->Bounds.GetBox();
			UpdateParams.ComponentToWorld = VectorFieldComponent->ComponentToWorld.ToMatrixWithScale();
			UpdateParams.Intensity = VectorFieldComponent->Intensity;
			UpdateParams.Tightness = VectorFieldComponent->Tightness;

			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
				FUpdateVectorFieldCommand,
				FFXSystem*, FXSystem, this,
				FVectorFieldInstance*, Instance, Instance,
				FUpdateVectorFieldParams, UpdateParams, UpdateParams,
			{
				Instance->WorldBounds = UpdateParams.Bounds;
				Instance->Intensity = UpdateParams.Intensity;
				Instance->Tightness = UpdateParams.Tightness;
				Instance->UpdateTransforms( UpdateParams.ComponentToWorld );
			});
		}
	}
}

/*-----------------------------------------------------------------------------
	Render related functionality.
-----------------------------------------------------------------------------*/

void FFXSystem::DrawDebug( FCanvas* Canvas )
{
	if (FXConsoleVariables::VisualizeGPUSimulation > 0
		&& RHISupportsGPUParticles(FeatureLevel))
	{
		VisualizeGPUParticles(Canvas);
	}
}

void FFXSystem::PreInitViews()
{
	if (RHISupportsGPUParticles(FeatureLevel))
	{
		AdvanceGPUParticleFrame();
	}
}

void FFXSystem::PreRender(FRHICommandListImmediate& RHICmdList)
{
	if (RHISupportsGPUParticles(FeatureLevel))
	{
		SimulateGPUParticles(RHICmdList, EParticleSimulatePhase::Main, NULL, FTexture2DRHIParamRef(), FTexture2DRHIParamRef());
	}
}

void FFXSystem::PostRenderOpaque(FRHICommandListImmediate& RHICmdList, const class FSceneView* CollisionView, FTexture2DRHIParamRef SceneDepthTexture, FTexture2DRHIParamRef GBufferATexture)
{
	if (RHISupportsGPUParticles(FeatureLevel))
	{
		SimulateGPUParticles(RHICmdList, EParticleSimulatePhase::Collision, CollisionView, SceneDepthTexture, GBufferATexture);
		SortGPUParticles(RHICmdList);
	}
}
