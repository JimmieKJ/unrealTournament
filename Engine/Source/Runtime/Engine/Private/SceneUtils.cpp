// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "RHI.h"
#include "SceneUtils.h"

#if WANTS_DRAW_MESH_EVENTS

template<typename TRHICmdList>
void TDrawEvent<TRHICmdList>::Start(TRHICmdList& InRHICmdList, const TCHAR* Fmt, ...)
{
	check(IsInParallelRenderingThread() || IsInRHIThread());
	{
		va_list ptr;
		va_start(ptr, Fmt);
		TCHAR TempStr[256];
		// Build the string in the temp buffer
		FCString::GetVarArgs(TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr) - 1, Fmt, ptr);
		InRHICmdList.PushEvent(TempStr);
		RHICmdList = &InRHICmdList;
	}
}

template<typename TRHICmdList>
void TDrawEvent<TRHICmdList>::Stop()
{
	if (RHICmdList)
	{
		RHICmdList->PopEvent();
		RHICmdList = NULL;
	}
}
template struct TDrawEvent<FRHICommandList>;
template struct TDrawEvent<FRHIAsyncComputeCommandList>;

void FDrawEventRHIExecute::Start(IRHIComputeContext& InRHICommandContext, const TCHAR* Fmt, ...)
{
	check(IsInParallelRenderingThread() || IsInRHIThread() || (!GRHIThread && IsInRenderingThread()));
	{
		
		va_list ptr;
		va_start(ptr, Fmt);
		TCHAR TempStr[256];
		// Build the string in the temp buffer
		FCString::GetVarArgs(TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr) - 1, Fmt, ptr);
		RHICommandContext = &InRHICommandContext;
		RHICommandContext->RHIPushEvent(TempStr);		
	}
}

void FDrawEventRHIExecute::Stop()
{
	RHICommandContext->RHIPopEvent();
}

#endif // WANTS_DRAW_MESH_EVENTS