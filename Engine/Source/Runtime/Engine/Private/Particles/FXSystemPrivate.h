// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	FXSystemPrivate.h: Internal effects system interface.
=============================================================================*/

#pragma once

#include "FXSystem.h"
#include "../VectorField.h"

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

/** An individual particle simulation taking place on the GPU. */
class FParticleSimulationGPU;
/** Resources used for particle simulation. */
class FParticleSimulationResources;

namespace EParticleSimulatePhase
{
	enum Type
	{
		/** The main simulation pass is for standard particles. */
		Main,
		/** The collision pass is used by these that collide against the scene depth buffer. */
		Collision,

		/**********************************************************************/

		/** The first simulation phase that is run each frame. */
		First = Main,
		/** The final simulation phase that is run each frame. */
		Last = Collision
	};
};

/*-----------------------------------------------------------------------------
	FX system declaration.
-----------------------------------------------------------------------------*/

/**
 * FX system.
 */
class FFXSystem : public FFXSystemInterface
{
public:

	/** Default constructoer. */
	FFXSystem(ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform);

	/** Destructor. */
	virtual ~FFXSystem();

	// Begin FFXSystemInterface.
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void Suspend() override;
	virtual void Resume() override;
#endif // #if WITH_EDITOR
	virtual void DrawDebug(FCanvas* Canvas) override;
	virtual void AddVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual void RemoveVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual void UpdateVectorField(UVectorFieldComponent* VectorFieldComponent) override;
	virtual FParticleEmitterInstance* CreateGPUSpriteEmitterInstance(FGPUSpriteEmitterInfo& EmitterInfo) override;
	virtual void PreInitViews() override;
	virtual void PreRender(FRHICommandListImmediate& RHICmdList) override;
	virtual void PostRenderOpaque(FRHICommandListImmediate& RHICmdList, const class FSceneView* CollisionView, FTexture2DRHIParamRef SceneDepthTexture, FTexture2DRHIParamRef GBufferATexture) override;
	// End FFXSystemInterface.

	/*--------------------------------------------------------------------------
		Internal interface for GPU simulation.
	--------------------------------------------------------------------------*/
	/**
	 * Retrieve feature level that this FXSystem was created for
	 */
	ERHIFeatureLevel::Type GetFeatureLevel() const { return FeatureLevel; }

	/**
	 * Retrieve shaderplatform that this FXSystem was created for
	 */
	EShaderPlatform GetShaderPlatform() const { return ShaderPlatform; }

	/**
	 * Add a new GPU simulation to the system.
	 * @param Simulation The GPU simulation to add.
	 */
	void AddGPUSimulation(FParticleSimulationGPU* Simulation);

	/**
	 * Remove an existing GPU simulation to the system.
	 * @param Simulation The GPU simulation to remove.
	 */
	void RemoveGPUSimulation(FParticleSimulationGPU* Simulation);

	/**
	 * Retrieve GPU particle rendering resources.
	 */
	FParticleSimulationResources* GetParticleSimulationResources()
	{
		return ParticleSimulationResources;
	}

	/**
	 * Prepares a GPU simulation to be sorted for a particular view.
	 * @param Simulation The simulation to be sorted.
	 * @param ViewOrigin The origin of the view from which to sort.
	 * @returns an offset in to the sorted buffer from which the simulation may render.
	 */
	int32 AddSortedGPUSimulation(FParticleSimulationGPU* Simulation, const FVector& ViewOrigin);

private:

	/*--------------------------------------------------------------------------
		Private interface for GPU simulations.
	--------------------------------------------------------------------------*/

	/**
	 * Initializes GPU simulation for this system.
	 */
	void InitGPUSimulation();

	/**
	 * Destroys any resources allocated for GPU simulation for this system.
	 */
	void DestroyGPUSimulation();

	/**
	 * Initializes GPU resources.
	 */
	void InitGPUResources();

	/**
	 * Releases GPU resources.
	 */
	void ReleaseGPUResources();

	/**
	 * Prepares GPU particles for simulation and rendering in the next frame.
	 */
	void AdvanceGPUParticleFrame();

	/**
	 * Sorts all GPU particles that have called AddSortedGPUSimulation since the
	 * last reset.
	 */
	void SortGPUParticles(FRHICommandListImmediate& RHICmdList);

	/**
	 * Update particles simulated on the GPU.
	 * @param Phase				Which emitters are being simulated.
	 * @param CollisionView		View to be used for collision checks.
	 * @param SceneDepthTexture Depth texture to use for collision checks.
	 * @param GBufferATexture	GBuffer texture containing the world normal.
	 */
	void SimulateGPUParticles(
		FRHICommandListImmediate& RHICmdList,
		EParticleSimulatePhase::Type Phase,
		const class FSceneView* CollisionView,
		FTexture2DRHIParamRef SceneDepthTexture,
		FTexture2DRHIParamRef GBufferATexture
		);

	/**
	 * Visualizes the current state of GPU particles.
	 * @param Canvas The canvas on which to draw the visualization.
	 */
	void VisualizeGPUParticles(FCanvas* Canvas);

private:

	/*-------------------------------------------------------------------------
		GPU simulation state.
	-------------------------------------------------------------------------*/

	/** List of all vector field instances. */
	FVectorFieldInstanceList VectorFields;
	/** List of all active GPU simulations. */
	TSparseArray<FParticleSimulationGPU*> GPUSimulations;
	/** Particle render resources. */
	FParticleSimulationResources* ParticleSimulationResources;
	/** Feature level of this effects system */
	ERHIFeatureLevel::Type FeatureLevel;
	/** Shader platform that will be rendering this effects system */
	EShaderPlatform ShaderPlatform;

#if WITH_EDITOR
	/** true if the system has been suspended. */
	bool bSuspended;
#endif // #if WITH_EDITOR
};
