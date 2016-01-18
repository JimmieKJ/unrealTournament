// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalGlobalUniformBuffer.h: Metal Global uniform definitions.
=============================================================================*/

#pragma once 

/** Size of the default constant buffer (copied from D3D11) */
#define MAX_GLOBAL_CONSTANT_BUFFER_SIZE		4096

/**
 * An Metal uniform buffer that has backing memory to store global uniforms
 */
class FMetalGlobalUniformBuffer : public FRenderResource, public FRefCountedObject
{
public:

	FMetalGlobalUniformBuffer(uint32 InSize = 0);
	~FMetalGlobalUniformBuffer();

	// FRenderResource interface.
	virtual void	InitDynamicRHI() override;
	virtual void	ReleaseDynamicRHI() override;

	
	void UpdateConstant(const void* Data, uint16 Offset, uint16 Size);

	void CommitToVertexShader(bool bDiscardSharedConstants);
	void CommitToFragmentShader(bool bDiscardSharedConstants);

protected:
	
	uint32 FinalizeBuffer(bool bDiscardSharedConstants);
	
	uint32	MaxSize;
	bool	bIsDirty;
	uint8*	ShadowData;

	/** Size of all constants that has been updated since the last call to Commit. */
	uint32	CurrentUpdateSize;

	/**
	 * Size of all constants that has been updated since the last Discard.
	 * Includes "shared" constants that don't necessarily gets updated between every Commit.
	 */
	uint32	TotalUpdateSize;
};
