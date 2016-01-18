// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

TGlobalResource<TBoundShaderStateHistory<10000>> FMetalRHICommandContext::BoundShaderStateHistory;

FMetalContext& GetMetalDeviceContext()
{
	FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
	check(Context);
	return Context->GetInternalContext();
}

void SafeReleaseMetalResource(id Object)
{
	if(GIsRHIInitialized && GDynamicRHI)
	{
		FMetalRHICommandContext* Context = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		if(Context)
		{
			Context->GetInternalContext().ReleaseObject(Object);
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
			Context->GetInternalContext().ReleasePooledBuffer(Buffer);
		}
	}
}

FMetalRHICommandContext::FMetalRHICommandContext(struct FMetalGPUProfiler* InProfiler)
: Context(new FMetalContext)
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

