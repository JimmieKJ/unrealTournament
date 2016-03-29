// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Represents a per instance data buffer for a custom Slate mesh element
 */
class FSlateUpdatableInstanceBuffer : public ISlateUpdatableInstanceBuffer
{
public:

	FSlateUpdatableInstanceBuffer( int32 InstanceCount );
	~FSlateUpdatableInstanceBuffer();
	void BindStreamSource(FRHICommandListImmediate& RHICmdList, int32 StreamIndex, uint32 InstanceOffset);

private:
	// BEGIN ISlateUpdatableInstanceBuffer
	virtual TSharedPtr<class FSlateInstanceBufferUpdate> BeginUpdate() override;
	virtual uint32 GetNumInstances() const override;
	virtual void UpdateRenderingData(int32 NumInstancesToUse) override;
	virtual TArray<FVector4>& GetBufferData() override;
	// END ISlateUpdatableInstanceBuffer

	void UpdateRenderingData_RenderThread(FRHICommandListImmediate& RHICmdList, int32 BufferIndex);
private:
	TArray<FVector4> BufferData[SlateRHIConstants::NumBuffers];
	TSlateElementVertexBuffer<FVector4> InstanceBufferResource;
	uint32 NumInstances;
	int32 FreeBufferIndex;
};



