// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "SlateElementIndexBuffer.h"

FSlateElementIndexBuffer::FSlateElementIndexBuffer()
	: BufferSize(0)	 
	, MinBufferSize(0)
	, BufferUsageSize(0)
{

}

FSlateElementIndexBuffer::~FSlateElementIndexBuffer()
{

}

void FSlateElementIndexBuffer::Init( int32 MinNumIndices )
{
	MinBufferSize = sizeof(SlateIndex) * FMath::Max( MinNumIndices, 200 );

	if ( IsInRenderingThread() )
	{
		InitResource();
	}
	else
	{
		BeginInitResource(this);
	}
}

void FSlateElementIndexBuffer::Destroy()
{
	if ( IsInRenderingThread() )
	{
		ReleaseResource();
	}
	else
	{
		BeginReleaseResource(this);
	}
}


/** Initializes the index buffers RHI resource. */
void FSlateElementIndexBuffer::InitDynamicRHI()
{
	checkSlow( IsInRenderingThread() );

	check( MinBufferSize > 0 );

	BufferSize = MinBufferSize;

	FRHIResourceCreateInfo CreateInfo;
	IndexBufferRHI = RHICreateIndexBuffer( sizeof(SlateIndex), MinBufferSize, BUF_Dynamic, CreateInfo );
	check( IsValidRef(IndexBufferRHI) );
}

/** Resizes the buffer to the passed in size.  Preserves internal data */
void FSlateElementIndexBuffer::ResizeBuffer( int32 NewSizeBytes )
{
	checkSlow( IsInRenderingThread() );

	int32 FinalSize = FMath::Max( NewSizeBytes, MinBufferSize );

	if( FinalSize != 0 && FinalSize != BufferSize )
	{
		IndexBufferRHI.SafeRelease();
		FRHIResourceCreateInfo CreateInfo;
		IndexBufferRHI = RHICreateIndexBuffer( sizeof(SlateIndex), FinalSize, BUF_Dynamic, CreateInfo );
		check(IsValidRef(IndexBufferRHI));

		BufferSize = FinalSize;
	}
}

void FSlateElementIndexBuffer::PreFillBuffer(int32 RequiredIndexCount, bool bShrinkToMinSize)
{
	//SCOPE_CYCLE_COUNTER( STAT_SlatePreFullBufferRTTime );

	checkSlow(IsInRenderingThread());

	if (RequiredIndexCount > 0)
	{
		int32 RequiredBufferSize = RequiredIndexCount*sizeof(SlateIndex);

		// resize if needed
		if (RequiredBufferSize > GetBufferSize() || bShrinkToMinSize)
		{
			// Use array resize techniques for the vertex buffer
			ResizeBuffer(RequiredBufferSize);
		}

		BufferUsageSize = RequiredBufferSize;		
	}
}

void* FSlateElementIndexBuffer::LockBuffer_RenderThread(int32 NumIndices)
{
	uint32 RequiredBufferSize = NumIndices*sizeof(SlateIndex);		
	return RHILockIndexBuffer( IndexBufferRHI, 0, RequiredBufferSize, RLM_WriteOnly );
}

void FSlateElementIndexBuffer::UnlockBuffer_RenderThread()
{
	RHIUnlockIndexBuffer( IndexBufferRHI );
}

void* FSlateElementIndexBuffer::LockBuffer_RHIThread(int32 NumIndices)
{
	uint32 RequiredBufferSize = NumIndices*sizeof(SlateIndex);		
	return GDynamicRHI->RHILockIndexBuffer( IndexBufferRHI, 0, RequiredBufferSize, RLM_WriteOnly );
}

void FSlateElementIndexBuffer::UnlockBuffer_RHIThread()
{
	GDynamicRHI->RHIUnlockIndexBuffer( IndexBufferRHI );
}

/** Releases the index buffers RHI resource. */
void FSlateElementIndexBuffer::ReleaseDynamicRHI()
{
	IndexBufferRHI.SafeRelease();
	BufferSize = 0;
}