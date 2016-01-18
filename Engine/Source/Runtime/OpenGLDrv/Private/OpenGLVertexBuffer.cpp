// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLVertexBuffer.cpp: OpenGL vertex buffer RHI implementation.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

namespace OpenGLConsoleVariables
{

	int32 bUseStagingBuffer = 1;

	static FAutoConsoleVariableRef CVarUseStagingBuffer(
		TEXT("OpenGL.UseStagingBuffer"),
		bUseStagingBuffer,
		TEXT("Enables maps of dynamic vertex buffers to go to a staging buffer"),
		ECVF_ReadOnly
		);
};

struct PoolAllocation
{
	uint8* Pointer;
	uint32 Size;
	uint32 Offset; // into the target buffer
	int32 FrameRetired;
};

static TArray<PoolAllocation*> AllocationList;
static TMap<void*,PoolAllocation*> AllocationMap;

static GLuint PoolVB = 0;
static uint8* PoolPointer = 0;
static uint32 FrameBytes = 0;
static uint32 FreeSpace = 0;
static uint32 OffsetVB = 0;
static const uint32 PerFrameMax = 1024*1024*4;

void* GetAllocation( void* Target, uint32 Size, uint32 Offset)
{
	if (FOpenGL::SupportsBufferStorage() && OpenGLConsoleVariables::bUseStagingBuffer)
	{
		if (PoolVB == 0)
		{
			FOpenGL::GenBuffers( 1, &PoolVB);
			glBindBuffer( GL_COPY_READ_BUFFER, PoolVB);
			FOpenGL::BufferStorage( GL_COPY_READ_BUFFER, PerFrameMax * 4, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
			PoolPointer = (uint8*)FOpenGL::MapBufferRange( GL_COPY_READ_BUFFER, 0, PerFrameMax * 4, FOpenGL::RLM_WriteOnlyPersistent );

			FreeSpace = PerFrameMax * 4;

			check(PoolPointer);
		}
		check (PoolVB);

		if (Size > PerFrameMax - FrameBytes || Size > FreeSpace)
		{
			return NULL;
		}

		if (Size > (PerFrameMax*4 - OffsetVB))
		{
			// We're wrapping, create dummy allocation and start at the begining
			uint32 Leftover = PerFrameMax*4 - OffsetVB;
			PoolAllocation* Alloc = new PoolAllocation;
			Alloc->Pointer = 0;
			Alloc->Offset = 0;
			Alloc->Size = Leftover;
			Alloc->FrameRetired = GFrameNumberRenderThread;

			AllocationList.Add(Alloc);
			OffsetVB = 0;
			FreeSpace -= Leftover;
		}

		//Again check if we have room
		if (Size > FreeSpace)
		{
			return NULL;
		}

		PoolAllocation* Alloc = new PoolAllocation;
		Alloc->Pointer = PoolPointer + OffsetVB;
		Alloc->Offset = Offset;
		Alloc->Size = Size;
		Alloc->FrameRetired = -1;

		AllocationList.Add(Alloc);
		AllocationMap.Add(Target,Alloc);
		OffsetVB += Size;
		FreeSpace -= Size;
		FrameBytes += Size;

		return Alloc->Pointer;

	}
	return NULL;
}

bool RetireAllocation( FOpenGLVertexBuffer* Target)
{
	if (FOpenGL::SupportsBufferStorage() && OpenGLConsoleVariables::bUseStagingBuffer)
	{
		PoolAllocation *Alloc = 0;

		if ( AllocationMap.RemoveAndCopyValue( Target, Alloc))
		{
			check( Alloc);
			Target->Bind();

			FOpenGL::CopyBufferSubData( GL_COPY_READ_BUFFER, GL_ARRAY_BUFFER, Alloc->Pointer - PoolPointer, Alloc->Offset, Alloc->Size);

			Alloc->FrameRetired = GFrameNumberRenderThread;

			return true;
		}
	}
	return false;
}

void BeginFrame_VertexBufferCleanup()
{
	if (GFrameNumberRenderThread < 3)
	{
		return;
	}

	int32 NumToRetire = 0;
	int32 FrameToRecover = GFrameNumberRenderThread - 3;

	while (NumToRetire < AllocationList.Num())
	{
		PoolAllocation *Alloc = AllocationList[NumToRetire];
		if (Alloc->FrameRetired < 0 || Alloc->FrameRetired > FrameToRecover)
		{
			break;
		}
		FreeSpace += Alloc->Size;
		delete Alloc;
		NumToRetire++;
	}

	AllocationList.RemoveAt(0,NumToRetire);
	FrameBytes = 0;
}

FVertexBufferRHIRef FOpenGLDynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	VERIFY_GL_SCOPE();

	const void *Data = NULL;

	// If a resource array was provided for the resource, create the resource pre-populated
	if(CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
		Data = CreateInfo.ResourceArray->GetResourceData();
	}

	TRefCountPtr<FOpenGLVertexBuffer> VertexBuffer = new FOpenGLVertexBuffer(0, Size, InUsage, Data);
	return VertexBuffer.GetReference();
}

void* FOpenGLDynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI,uint32 Offset,uint32 Size,EResourceLockMode LockMode)
{
	check(Size > 0);

	VERIFY_GL_SCOPE();
	FOpenGLVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if( !(FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) && VertexBuffer->GetUsage() & BUF_ZeroStride )
	{
		check( Offset + Size <= VertexBuffer->GetSize() );
		// We assume we are only using the first elements from the VB, so we can later on read this memory and create an expanded version of this zero stride buffer
		check(Offset == 0);
		return (void*)( (uint8*)VertexBuffer->GetZeroStrideBuffer() + Offset );
	}
	else
	{
		if (VertexBuffer->IsDynamic() && LockMode == RLM_WriteOnly)
		{
			void *Staging = GetAllocation(VertexBuffer, Size, Offset);
			if (Staging)
			{
				return Staging;
			}
		}
		return VertexBuffer->Lock(Offset, Size, LockMode == RLM_ReadOnly, VertexBuffer->IsDynamic());
	}
}

void FOpenGLDynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	VERIFY_GL_SCOPE();
	FOpenGLVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	if( (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB) || !( VertexBuffer->GetUsage() & BUF_ZeroStride ) )
	{
		if (!RetireAllocation(VertexBuffer))
		{
			VertexBuffer->Unlock();
		}
	}
}

void FOpenGLDynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI,FVertexBufferRHIParamRef DestBufferRHI)
{
	VERIFY_GL_SCOPE();
	check( FOpenGL::SupportsCopyBuffer() );
	FOpenGLVertexBuffer* SourceBuffer = ResourceCast(SourceBufferRHI);
	FOpenGLVertexBuffer* DestBuffer = ResourceCast(DestBufferRHI);
	check(SourceBuffer->GetSize() == DestBuffer->GetSize());

	glBindBuffer(GL_COPY_READ_BUFFER,SourceBuffer->Resource);
	glBindBuffer(GL_COPY_WRITE_BUFFER,DestBuffer->Resource);
	FOpenGL::CopyBufferSubData(GL_COPY_READ_BUFFER,GL_COPY_WRITE_BUFFER,0,0,SourceBuffer->GetSize());
	glBindBuffer(GL_COPY_READ_BUFFER,0);
	glBindBuffer(GL_COPY_WRITE_BUFFER,0);
}

