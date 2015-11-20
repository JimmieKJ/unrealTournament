// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RHIPrivateUtil.h: Private D3D RHI Utility definitions for WinRT.
=============================================================================*/

#pragma once

struct FD3DRHIUtil
{
	template <EShaderFrequency ShaderFrequencyT>
	static FORCEINLINE void CommitConstants(FD3D12ConstantBuffer* InConstantBuffer, FD3D12StateCache& StateCache, uint32 Index, bool bDiscardSharedConstants)
	{
		auto* ConstantBuffer = ((FWinD3D12ConstantBuffer*)InConstantBuffer);
		// Array may contain NULL entries to pad out to proper 
		if (ConstantBuffer && ConstantBuffer->CommitConstantsToDevice(bDiscardSharedConstants))
		{
			ID3D12Resource* DeviceBuffer = ConstantBuffer->GetConstantBuffer();
			StateCache.SetConstantBuffer<ShaderFrequencyT>(DeviceBuffer, Index);
		}
	}
};
