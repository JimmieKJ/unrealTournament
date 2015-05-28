// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

class FSlateRHIResourceManager;

#include "RenderingPolicy.h"

/** 
 * Vertex buffer containing all Slate vertices
 * All vertices are added through RHILockVertexBuffer
 */
template <typename VertexType>
class TSlateElementVertexBuffer : public FVertexBuffer
{
public:
	TSlateElementVertexBuffer()
		: BufferSize(0)
		, BufferUsageSize(0)
	{}

	TSlateElementVertexBuffer( uint32 InBufferSize )
		: BufferSize(InBufferSize)
		, BufferUsageSize(0)
	{}

	~TSlateElementVertexBuffer() {};

	/** Initializes the vertex buffers RHI resource. */
	virtual void InitDynamicRHI()
	{
		if( !IsValidRef(VertexBufferRHI) )
		{
			if( BufferSize == 0 )
			{
				BufferSize = 200 * sizeof(VertexType);
			}

			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer( BufferSize, BUF_Dynamic, CreateInfo );

			// Ensure the vertex buffer could be created
			check(IsValidRef(VertexBufferRHI));
		}
	}

	/** Releases the vertex buffers RHI resource. */
	virtual void ReleaseDynamicRHI()
	{
		VertexBufferRHI.SafeRelease();
		BufferSize = 0;
	}

	/** Returns a friendly name for this buffer. */
	virtual FString GetFriendlyName() const { return TEXT("SlateElementVertices"); }

	/** Returns the size of the buffer in bytes. */
	uint32 GetBufferSize() const { return BufferSize; }

	/** Returns the used size of this buffer */
	uint32 GetBufferUsageSize() const { return BufferUsageSize; }

	/** Resets the usage of the buffer */
	void ResetBufferUsage() { BufferUsageSize = 0; }

	/** Fills the buffer with slate vertices */
	void FillBuffer( const TArray<VertexType>& InVertices, bool bShrinkToFit )
	{
		check( IsInRenderingThread() );

		if( InVertices.Num() )
		{
			uint32 NumVertices = InVertices.Num();

	#if !SLATE_USE_32BIT_INDICES
			// make sure our index buffer can handle this
			checkf(NumVertices < 0xFFFF, TEXT("Slate vertex buffer is too large (%d) to work with uint16 indices"), NumVertices);
	#endif

			uint32 RequiredBufferSize = NumVertices*sizeof(VertexType);

			// resize if needed
			if( RequiredBufferSize > GetBufferSize() || bShrinkToFit )
			{
				// Use array resize techniques for the vertex buffer
				ResizeBuffer( InVertices.GetAllocatedSize() );
			}

			BufferUsageSize += RequiredBufferSize;

			void* VerticesPtr = RHILockVertexBuffer( VertexBufferRHI, 0, RequiredBufferSize, RLM_WriteOnly );

			FMemory::Memcpy( VerticesPtr, InVertices.GetData(), RequiredBufferSize );

			RHIUnlockVertexBuffer( VertexBufferRHI );
		}
	}

private:
	/** Resizes the buffer to the passed in size.  Preserves internal data*/
	void ResizeBuffer( uint32 NewSizeBytes )
	{
		check( IsInRenderingThread() );

		if( NewSizeBytes != 0 && NewSizeBytes != BufferSize)
		{
			VertexBufferRHI.SafeRelease();

			FRHIResourceCreateInfo CreateInfo;
			VertexBufferRHI = RHICreateVertexBuffer(NewSizeBytes, BUF_Dynamic, CreateInfo);

			check(IsValidRef(VertexBufferRHI));

			BufferSize = NewSizeBytes;
		}
	}

private:
	/** The size of the buffer in bytes. */
	uint32 BufferSize;
	
	/** The size of the used portion of the buffer */
	uint32 BufferUsageSize;

	/** Hidden copy methods. */
	TSlateElementVertexBuffer( const TSlateElementVertexBuffer& );
	void operator=(const TSlateElementVertexBuffer& );
};

class FSlateElementIndexBuffer : public FIndexBuffer
{
public:
	FSlateElementIndexBuffer();
	~FSlateElementIndexBuffer();

	/** Initializes the index buffers RHI resource. */
	virtual void InitDynamicRHI() override;

	/** Releases the index buffers RHI resource. */
	virtual void ReleaseDynamicRHI() override;

	/** Returns a friendly name for this buffer. */
	virtual FString GetFriendlyName() const override { return TEXT("SlateElementIndices"); }

	/** Returns the size of this buffer */
	uint32 GetBufferSize() const { return BufferSize; }

	/** Returns the used size of this buffer */
	uint32 GetBufferUsageSize() const { return BufferUsageSize; }

	/** Resets the usage of the buffer */
	void ResetBufferUsage() { BufferUsageSize = 0; }

	/** Fills the buffer with slate vertices */
	void FillBuffer( const TArray<SlateIndex>& InIndices, bool bShrinkToFit );
private:
	/** Resizes the buffer to the passed in size.  Preserves internal data */
	void ResizeBuffer( uint32 NewSizeBytes );
private:
	/** The size of the buffer in bytes. */
	uint32 BufferSize;
	
	/** The size of the used portion of the buffer */
	uint32 BufferUsageSize;

	/** Hidden copy methods. */
	FSlateElementIndexBuffer( const FSlateElementIndexBuffer& );
	void operator=(const FSlateElementIndexBuffer& );

};

class FSlateRHIRenderingPolicy : public FSlateRenderingPolicy
{
public:
	FSlateRHIRenderingPolicy( TSharedPtr<FSlateFontCache> InFontCache, TSharedRef<FSlateRHIResourceManager> InResourceManager );
	~FSlateRHIRenderingPolicy();

	virtual void UpdateBuffers( const FSlateWindowElementList& WindowElementList ) override;
	virtual void DrawElements(FRHICommandListImmediate& RHICmdList, class FSlateBackBuffer& BackBuffer, const FMatrix& ViewProjectionMatrix, const TArray<FSlateRenderBatch>& RenderBatches, bool bAllowSwtichVerticalAxis=true);

	virtual TSharedRef<FSlateFontCache> GetFontCache() override { return FontCache.ToSharedRef(); }
	virtual TSharedRef<FSlateShaderResourceManager> GetResourceManager() override { return ResourceManager; }
	
	void InitResources();
	void ReleaseResources();

	void BeginDrawingWindows();
	void EndDrawingWindows();

	void SetUseGammaCorrection( bool bInUseGammaCorrection ) { bGammaCorrect = bInUseGammaCorrection; }
private:
	/**
	 * Returns the pixel shader that should be used for the specified ShaderType and DrawEffects
	 * 
	 * @param ShaderType	The shader type being used
	 * @param DrawEffects	Draw effects being used
	 * @return The pixel shader for use with the shader type and draw effects
	 */
	class FSlateElementPS* GetTexturePixelShader( ESlateShader::Type ShaderType, ESlateDrawEffect::Type DrawEffects );
	class FSlateMaterialShaderPS* GetMaterialPixelShader( const class FMaterial* Material, ESlateShader::Type ShaderType, ESlateDrawEffect::Type DrawEffects );

	/** @return The RHI primitive type from the Slate primitive type */
	EPrimitiveType GetRHIPrimitiveType(ESlateDrawPrimitive::Type SlateType);
private:
	/** Buffers used for rendering */
	TSlateElementVertexBuffer<FSlateVertex> VertexBuffers[2];
	FSlateElementIndexBuffer IndexBuffers[2];

	TSharedRef<FSlateRHIResourceManager> ResourceManager;
	TSharedPtr<FSlateFontCache> FontCache;

	uint8 CurrentBufferIndex;
	
	/** If we should shrink resources that are no longer used (do not set this from the game thread)*/
	bool bShouldShrinkResources;

	bool bGammaCorrect;
};


