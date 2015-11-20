// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

class FSlateRHIResourceManager;

#include "RenderingPolicy.h"

class FSlateRHIRenderingPolicy : public FSlateRenderingPolicy
{
public:
	FSlateRHIRenderingPolicy( TSharedPtr<FSlateFontCache> InFontCache, TSharedRef<FSlateRHIResourceManager> InResourceManager );
	~FSlateRHIRenderingPolicy();

	void UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& BatchData);
	void UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& BatchData, const TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe>& RenderHandle);

	void ReleaseCachedRenderData(FSlateRenderDataHandle* InRenderHandle);
	void ReleaseCachingResourcesFor(const ILayoutCache* Cacher);

	virtual void DrawElements(FRHICommandListImmediate& RHICmdList, class FSlateBackBuffer& BackBuffer, const FMatrix& ViewProjectionMatrix, const TArray<FSlateRenderBatch>& RenderBatches, bool bAllowSwtichVerticalAxis=true);

	virtual TSharedRef<FSlateFontCache> GetFontCache() override { return FontCache.ToSharedRef(); }
	virtual TSharedRef<FSlateShaderResourceManager> GetResourceManager() override { return ResourceManager; }
	virtual bool IsVertexColorInLinearSpace() const override { return false; }
	
	void InitResources();
	void ReleaseResources();

	void BeginDrawingWindows();
	void EndDrawingWindows();

	void SetUseGammaCorrection( bool bInUseGammaCorrection ) { bGammaCorrect = bInUseGammaCorrection; }

protected:
	void UpdateVertexAndIndexBuffers(FRHICommandListImmediate& RHICmdList, FSlateBatchData& BatchData, TSlateElementVertexBuffer<FSlateVertex>& VertexBuffer, FSlateElementIndexBuffer& IndexBuffer);

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
	class FSlateMaterialShaderVS* GetMaterialVertexShader( const class FMaterial* Material, bool bUseInstancing );

	/** @return The RHI primitive type from the Slate primitive type */
	EPrimitiveType GetRHIPrimitiveType(ESlateDrawPrimitive::Type SlateType);

	/**
	 * Delete resources that have had their rendering resources released
	 */
	void DeleteReleasedResources();
private:
	/** Buffers used for rendering */
	TSlateElementVertexBuffer<FSlateVertex> VertexBuffers[SlateRHIConstants::NumBuffers];
	FSlateElementIndexBuffer IndexBuffers[SlateRHIConstants::NumBuffers];

	TSharedRef<FSlateRHIResourceManager> ResourceManager;
	TSharedPtr<FSlateFontCache> FontCache;

	bool bGammaCorrect;

	uint8 CurrentBufferIndex;
};


