// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

TGlobalResource<TBoundShaderStateHistory<10000>> FMetalRHICommandContext::BoundShaderStateHistory;

FMetalDeviceContext& GetMetalDeviceContext()
{
	FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
	check(Context);
	return ((FMetalDeviceContext&)Context->GetInternalContext());
}

void SafeReleaseMetalResource(id Object)
{
	if(GIsRHIInitialized && GDynamicRHI)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleaseObject(Object);
			return;
		}
	}
	[Object release];
}

void SafeReleasePooledBuffer(id<MTLBuffer> Buffer)
{
	if(GIsRHIInitialized && GDynamicRHI)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			((FMetalDeviceContext&)Context->GetInternalContext()).ReleasePooledBuffer(Buffer);
		}
	}
}

FMetalRHICommandContext::FMetalRHICommandContext(struct FMetalGPUProfiler* InProfiler, FMetalContext* WrapContext)
: Context(WrapContext)
, Profiler(InProfiler)
, PendingVertexBufferOffset(0xFFFFFFFF)
, PendingVertexDataStride(0)
, PendingIndexBufferOffset(0xFFFFFFFF)
, PendingIndexDataStride(0)
, PendingPrimitiveType(0)
, PendingNumPrimitives(0)
{
	check(Context);
}

FMetalRHICommandContext::~FMetalRHICommandContext()
{
	delete Context;
}

FMetalRHIComputeContext::FMetalRHIComputeContext(struct FMetalGPUProfiler* InProfiler, FMetalContext* WrapContext)
: FMetalRHICommandContext(InProfiler, WrapContext)
{
}

FMetalRHIComputeContext::~FMetalRHIComputeContext()
{
}

void FMetalRHIComputeContext::RHISetAsyncComputeBudget(EAsyncComputeBudget Budget)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false);
	}
	FMetalRHICommandContext::RHISetAsyncComputeBudget(Budget);
}

void FMetalRHIComputeContext::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShader)
{
	if (!Context->GetCurrentCommandBuffer())
	{
		Context->InitFrame(false);
	}
	FMetalRHICommandContext::RHISetComputeShader(ComputeShader);
}

void FMetalRHIComputeContext::RHISubmitCommandsHint()
{
	Context->FinishFrame();
	
	FMetalContext::MakeCurrent(&GetMetalDeviceContext());
	FMetalContext::CreateAutoreleasePool();
}
