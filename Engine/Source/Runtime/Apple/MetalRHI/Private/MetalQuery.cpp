// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalQuery.cpp: Metal query RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"

FMetalQueryBuffer::FMetalQueryBuffer(FMetalContext* InContext, id<MTLBuffer> InBuffer)
: Pool(InContext->GetQueryBufferPool())
, Buffer(InBuffer)
, WriteOffset(0)
{
}

FMetalQueryBuffer::~FMetalQueryBuffer()
{
	if(Buffer && GIsRHIInitialized)
    {
		TSharedPtr<FMetalQueryBufferPool, ESPMode::ThreadSafe> BufferPool = Pool.Pin();
		if (BufferPool.IsValid())
		{
			BufferPool->ReleaseQueryBuffer(Buffer);
		}
		else
		{
			[Buffer release];
		}
    }
}

uint64 FMetalQueryBuffer::GetResult(uint32 Offset)
{
    check(Buffer);
	uint64 Result = 0;
	@autoreleasepool
	{
		Result = *((uint64 const*)(((uint8*)[Buffer contents]) + Offset));
	}
    return Result;
}

bool FMetalCommandBufferFence::Wait(uint64 Millis)
{
	@autoreleasepool
	{
		TSharedPtr<MTLCommandBufferRef, ESPMode::ThreadSafe> CommandBuffer = CommandBufferRef.Pin();
		if (CommandBuffer.IsValid())
		{
			if(Millis > 0 && [(*CommandBuffer) status] < MTLCommandBufferStatusCompleted)
			{
				check([(*CommandBuffer) status] >= MTLCommandBufferStatusCommitted && [(*CommandBuffer) status] <= MTLCommandBufferStatusError);
				[(*CommandBuffer) waitUntilCompleted];
				check([(*CommandBuffer) status] >= MTLCommandBufferStatusCommitted && [(*CommandBuffer) status] <= MTLCommandBufferStatusError);
			}
			if([(*CommandBuffer) status] == MTLCommandBufferStatusError)
			{
				FMetalCommandList::HandleMetalCommandBufferFailure((*CommandBuffer));
			}
			return ([(*CommandBuffer) status] >= MTLCommandBufferStatusCompleted);
		}
		else
		{
			return true;
		}
	}
}
	
bool FMetalQueryResult::Wait(uint64 Millis)
{
	if(IsValidRef(SourceBuffer))
	{
		if(!bCompleted)
		{
			bCompleted = CommandBufferFence.Wait(Millis);
			
			return bCompleted;
		}
		return true;
	}
	return false;
}

uint64 FMetalQueryResult::GetResult()
{
	if(IsValidRef(SourceBuffer))
	{
		return SourceBuffer->GetResult(Offset);
	}
	return 0;
}

FMetalRenderQuery::FMetalRenderQuery(ERenderQueryType InQueryType)
: Type(InQueryType)
{
	Buffer.Offset = 0;
	Buffer.bCompleted = false;
}

FMetalRenderQuery::~FMetalRenderQuery()
{
	Buffer.SourceBuffer.SafeRelease();
	Buffer.Offset = 0;
}

void FMetalRenderQuery::Begin(FMetalContext* Context)
{
	Buffer.SourceBuffer.SafeRelease();
	Buffer.Offset = 0;
	
	if(Type == RQT_Occlusion)
	{
		// these only need to be 8 byte aligned (of course, a normal allocation oafter this will be 256 bytes, making large holes if we don't do all
		// query allocations at once)
		Context->GetQueryBufferPool()->Allocate(Buffer);
		Buffer.bCompleted = false;
		
		Result = 0;
		bAvailable = false;
		
		if ((GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM4) && GetMetalDeviceContext().SupportsFeature(EMetalFeaturesCountingQueries))
		{
			Context->GetCommandEncoder().SetVisibilityResultMode(MTLVisibilityResultModeCounting, Buffer.Offset);
		}
		else
		{
			Context->GetCommandEncoder().SetVisibilityResultMode(MTLVisibilityResultModeBoolean, Buffer.Offset);
		}
	}
}

void FMetalRenderQuery::End(FMetalContext* Context)
{
	if(Type == RQT_Occlusion)
	{
		// switch back to non-occlusion rendering
		Context->GetCommandEncoder().SetVisibilityResultMode(MTLVisibilityResultModeDisabled, 0);
		Context->InsertCommandBufferFence(Buffer.CommandBufferFence);
	}
}

FRenderQueryRHIRef FMetalDynamicRHI::RHICreateRenderQuery_RenderThread(class FRHICommandListImmediate& RHICmdList, ERenderQueryType QueryType)
{
	return GDynamicRHI->RHICreateRenderQuery(QueryType);
}

FRenderQueryRHIRef FMetalDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	return new FMetalRenderQuery(QueryType);
}

bool FMetalDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutNumPixels,bool bWait)
{
	check(IsInRenderingThread());
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);

    if(!Query->bAvailable)
    {
		SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );
		
		bool bOK = false;
		
		if (bWait)
		{
			uint32 IdleStart = FPlatformTime::Cycles();
		
			uint64 WaitMS = 500;
			bOK = Query->Buffer.Wait(WaitMS);
			
			GRenderThreadIdle[ERenderThreadIdleTypes::WaitingForGPUQuery] += FPlatformTime::Cycles() - IdleStart;
			GRenderThreadNumIdle[ERenderThreadIdleTypes::WaitingForGPUQuery]++;
		}
		else
		{
			bOK = Query->Buffer.Wait(0);
		}
		
        Query->Result = 0;
		Query->bAvailable = Query->Buffer.bCompleted; // @todo Zebra Never wait for a failed signal again.
		
        if (bOK == false)
        {
			UE_LOG(LogMetal, Log, TEXT("Timed out while waiting for GPU to catch up. (0.5f s)"));
			return false;
        }
		
		if(Query->Type == RQT_Occlusion)
		{
			Query->Result = Query->Buffer.GetResult();
		}
		Query->Buffer.SourceBuffer.SafeRelease();
    }

    // at this point, we are ready to read the value!
    OutNumPixels = Query->Result;
    return true;
}

// Occlusion/Timer queries.
void FMetalRHICommandContext::RHIBeginRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);
	
	Query->Begin(Context);
}

void FMetalRHICommandContext::RHIEndRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);
	
	Query->End(Context);
}

void FMetalRHICommandContext::RHIBeginOcclusionQueryBatch()
{
}

void FMetalRHICommandContext::RHIEndOcclusionQueryBatch()
{
}
