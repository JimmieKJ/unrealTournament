// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RHIPrivateUtil.h: Private D3D RHI Utility definitions for Windows.
=============================================================================*/

#pragma once

#include "WindowsD3D12ConstantBuffer.h"

struct FD3DRHIUtil
{
	template <EShaderFrequency ShaderFrequencyT>
	static FORCEINLINE void CommitConstants(FD3D12DynamicHeapAllocator& UploadHeap, FD3D12ConstantBuffer* InConstantBuffer, FD3D12StateCache& StateCache, uint32 Index, bool bDiscardSharedConstants)
	{
		auto* ConstantBuffer = ((FWinD3D12ConstantBuffer*)InConstantBuffer);
		// Array may contain NULL entries to pad out to proper 
        if (ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(UploadHeap, bDiscardSharedConstants))
		{
			FD3D12ResourceLocation* DeviceBufferLocation = ConstantBuffer->GetConstantBufferLocation();
			StateCache.SetConstantBuffer<ShaderFrequencyT>(Index, DeviceBufferLocation, nullptr);
		}
	}
};
