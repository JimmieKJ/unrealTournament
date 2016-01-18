// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsD3D12ConstantBuffer.h: D3D Constant Buffer functions
=============================================================================*/

#pragma once

class FWinD3D12ConstantBuffer : public FD3D12ConstantBuffer
{
public:
	FWinD3D12ConstantBuffer(FD3D12Device* InParent, uint32 InSize = 0, uint32 SubBuffers = 1) :
        FD3D12ConstantBuffer(InParent, InSize, SubBuffers)
	{
	}

	// FRenderResource interface.
	virtual void	InitDynamicRHI() override;
	virtual void	ReleaseDynamicRHI() override;

	/**
	* Get the current buffer
	*/
	FD3D12ResourceLocation* GetConstantBufferLocation() const
	{
		return CurrentBuffer.GetReference();
	}

	/**
	* Unlocks the constant buffer so the data can be transmitted to the device
	*/
    bool CommitConstantsToDevice(FD3D12DynamicHeapAllocator& UploadHeap, bool bDiscardSharedConstants);

private:
    TRefCountPtr<FD3D12ResourceLocation> CurrentBuffer;
};
