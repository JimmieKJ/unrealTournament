// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessAA.cpp: Post processing input implementation.
=============================================================================*/

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "PostProcessInput.h"

FRCPassPostProcessInput::FRCPassPostProcessInput(TRefCountPtr<IPooledRenderTarget>& InData)
	: Data(InData)
{
}

void FRCPassPostProcessInput::Process(FRenderingCompositePassContext& Context)
{
	PassOutputs[0].PooledRenderTarget = Data;
}

FPooledRenderTargetDesc FRCPassPostProcessInput::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	check(Data);

	FPooledRenderTargetDesc Ret = Data->GetDesc();

	return Ret;
}

void FRCPassPostProcessInputSingleUse::Process(FRenderingCompositePassContext& Context)
{
	PassOutputs[0].PooledRenderTarget = Data;

	// to get the RT freed up earlier than when the graph is cleaned up
	Data.SafeRelease();
}
