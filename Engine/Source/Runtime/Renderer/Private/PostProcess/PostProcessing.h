// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessing.h: The center for all post processing activities.
=============================================================================*/

#pragma once

#include "RenderingCompositionGraph.h"
#include "PostProcessInput.h"
#include "PostProcessOutput.h"

/** The context used to setup a post-process pass. */
class FPostprocessContext
{
public:

	FPostprocessContext(FRHICommandListImmediate& InRHICmdList, FRenderingCompositionGraph& InGraph, const FViewInfo& InView);

	FRHICommandListImmediate& RHICmdList;
	FRenderingCompositionGraph& Graph;
	const FViewInfo& View;

	// 0 if there was no scene color available at constructor call time
	FRenderingCompositePass* SceneColor;
	// never 0
	FRenderingCompositePass* SceneDepth;

	FRenderingCompositeOutputRef FinalOutput;
};

/** Encapsulates the post processing vertex shader. */
class FPostProcessVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessVS,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	/** Default constructor. */
	FPostProcessVS() {}

	/** to have a similar interface as all other shaders */
	void SetParameters(const FRenderingCompositePassContext& Context)
	{
		FGlobalShader::SetParameters(Context.RHICmdList, GetVertexShader(), Context.View);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View)
	{
		FGlobalShader::SetParameters(RHICmdList, GetVertexShader(), View);
	}

public:

	/** Initialization constructor. */
	FPostProcessVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}
};



/**
 * The center for all post processing activities.
 */
class FPostProcessing
{
public:

	bool AllowFullPostProcessing(const FViewInfo& View, ERHIFeatureLevel::Type FeatureLevel);

	// @param VelocityRT only valid if motion blur is supported
	void Process(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& VelocityRT);

	void ProcessES2(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, bool bViewRectSource);

	void ProcessPlanarReflection(FRHICommandListImmediate& RHICmdList, FViewInfo& View, TRefCountPtr<IPooledRenderTarget>& VelocityRT, TRefCountPtr<IPooledRenderTarget>& OutFilteredSceneColor);
};

/** The global used for post processing. */
extern FPostProcessing GPostProcessing;
