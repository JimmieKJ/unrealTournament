// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalQuery.cpp: Metal query RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"

FMetalRenderQuery::FMetalRenderQuery(ERenderQueryType InQueryType)
{

}

FMetalRenderQuery::~FMetalRenderQuery()
{

}


void FMetalRenderQuery::Begin()
{
	// these only need to be 8 byte aligned (of course, a normal allocation oafter this will be 256 bytes, making large holes if we don't do all 
	// query allocations at once)
	Offset = FMetalManager::Get()->AllocateFromQueryBuffer();

	// reset the query to 0
	uint32* QueryMemory = (uint32*)((uint8*)[FMetalManager::Get()->GetQueryBuffer() contents] + Offset);
	*QueryMemory = 0;

	// remember which command buffer we are in
	QueryCompleteEvent = FMetalManager::Get()->GetCurrentQueryCompleteEvent();

	[FMetalManager::GetContext() setVisibilityResultMode:MTLVisibilityResultModeBoolean offset:Offset];
}

void FMetalRenderQuery::End()
{
	// switch back to non-occlusion rendering
	[FMetalManager::GetContext() setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
}



FRenderQueryRHIRef FMetalDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	return new FMetalRenderQuery(QueryType);
}

bool FMetalDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutNumPixels,bool bWait)
{
	check(IsInRenderingThread());
	FMetalRenderQuery* Query = ResourceCast(QueryRHI);

	uint32* QueryMemory = (uint32*)((uint8*)[FMetalManager::Get()->GetQueryBuffer() contents] + Query->Offset);

	if (bWait)
	{
		// wait for half a second
		uint64 WaitTimeMS = 500;
		Query->QueryCompleteEvent->Wait(WaitTimeMS);
	}

	// waiting for 0 time on a manual reset event will return true if it was already triggered
 	if (Query->QueryCompleteEvent->Wait(0) == false)
 	{
 		return false;
 	}

	// at this point, we are ready to read the value!
	OutNumPixels = *QueryMemory;
	return true;
}

void FMetalDynamicRHI::RHIBeginOcclusionQueryBatch()
{
}

void FMetalDynamicRHI::RHIEndOcclusionQueryBatch()
{
}
