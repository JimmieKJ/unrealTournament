// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIRendererPrivatePCH.h"
#include "SlateUpdatableBuffer.h"

DECLARE_CYCLE_STAT(TEXT("UpdateInstanceBuffer Time"), STAT_SlateUpdateInstanceBuffer, STATGROUP_Slate);

struct FSlateUpdateInstanceBufferCommand : public FRHICommand<FSlateUpdateInstanceBufferCommand>
{
	TSlateElementVertexBuffer<FVector4>& InstanceBuffer;
	const TArray<FVector4>& InstanceData;

	FSlateUpdateInstanceBufferCommand(TSlateElementVertexBuffer<FVector4>& InInstanceBuffer, const TArray<FVector4>& InInstanceData )
		: InstanceBuffer(InInstanceBuffer)
		, InstanceData(InInstanceData)
	{}

	void Execute(FRHICommandListBase& CmdList)
	{
		SCOPE_CYCLE_COUNTER( STAT_SlateUpdateInstanceBuffer );

		uint8* InstanceBufferData = (uint8*)InstanceBuffer.LockBuffer_RHIThread(InstanceData.Num());

		FMemory::Memcpy(InstanceBufferData, InstanceData.GetData(), InstanceData.Num()*sizeof(FVector4) );
	
		InstanceBuffer.UnlockBuffer_RHIThread();
	}
};

FSlateUpdatableInstanceBuffer::FSlateUpdatableInstanceBuffer( int32 InitialInstanceCount )
	: FreeBufferIndex(0)
{
	InstanceBufferResource.Init(InitialInstanceCount);
	for( int32 BufferIndex = 0; BufferIndex < SlateRHIConstants::NumBuffers; ++BufferIndex )
	{
		BufferData[BufferIndex].Reserve( InitialInstanceCount );
	}

}

FSlateUpdatableInstanceBuffer::~FSlateUpdatableInstanceBuffer()
{
	InstanceBufferResource.Destroy();
	FlushRenderingCommands();
}

void FSlateUpdatableInstanceBuffer::BindStreamSource(FRHICommandListImmediate& RHICmdList, int32 StreamIndex, uint32 InstanceOffset)
{
	RHICmdList.SetStreamSource(StreamIndex, InstanceBufferResource.VertexBufferRHI, sizeof(FVector4), InstanceOffset*sizeof(FVector4));
}

TSharedPtr<class FSlateInstanceBufferUpdate> FSlateUpdatableInstanceBuffer::BeginUpdate()
{
	return MakeShareable(new FSlateInstanceBufferUpdate(*this));
}

uint32 FSlateUpdatableInstanceBuffer::GetNumInstances() const
{
	return NumInstances;
}

void FSlateUpdatableInstanceBuffer::UpdateRenderingData(int32 NumInstancesToUse)
{
	NumInstances = NumInstancesToUse;
	if (NumInstances > 0)
	{
		
		// Enqueue a command to unlock the draw buffer after all windows have been drawn
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER( SlateBeginDrawingWindowsCommand, 
			FSlateUpdatableInstanceBuffer&, Self, *this,
			int32, BufferIndex, FreeBufferIndex,
		{
			Self.UpdateRenderingData_RenderThread(RHICmdList, BufferIndex);
		});
	
		FreeBufferIndex = (FreeBufferIndex + 1) % SlateRHIConstants::NumBuffers;
	}
}

TArray<FVector4>& FSlateUpdatableInstanceBuffer::GetBufferData()
{
	return BufferData[FreeBufferIndex];
}

void FSlateUpdatableInstanceBuffer::UpdateRenderingData_RenderThread(FRHICommandListImmediate& RHICmdList, int32 BufferIndex)
{
	SCOPE_CYCLE_COUNTER( STAT_SlateUpdateInstanceBuffer );

	const TArray<FVector4>& RenderThreadBufferData = BufferData[BufferIndex];
	InstanceBufferResource.PreFillBuffer( RenderThreadBufferData.Num(), false );

	if(!GRHIThread || RHICmdList.Bypass())
	{
		uint8* InstanceBufferData = (uint8*)InstanceBufferResource.LockBuffer_RHIThread(RenderThreadBufferData.Num());

		FMemory::Memcpy(InstanceBufferData, RenderThreadBufferData.GetData(), RenderThreadBufferData.Num()*sizeof(FVector4) );
	
		InstanceBufferResource.UnlockBuffer_RHIThread();
	}
	else
	{
		new (RHICmdList.AllocCommand<FSlateUpdateInstanceBufferCommand>()) FSlateUpdateInstanceBufferCommand(InstanceBufferResource, RenderThreadBufferData);
	}
}

