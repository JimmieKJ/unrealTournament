// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

class FSlateOpenGLTextureCache;

class FSlateOpenGLRenderingPolicy : public FSlateRenderingPolicy
{
public:
	FSlateOpenGLRenderingPolicy( TSharedPtr<FSlateFontCache>& InFontCache, TSharedPtr<FSlateOpenGLTextureManager>& InTextureManager );
	~FSlateOpenGLRenderingPolicy();

	/**
	 * Updates vertex and index buffers used in drawing
	 *
	 * @param BatchData	The batch data that contains rendering data we need to upload to the buffers
	 */
	void UpdateVertexAndIndexBuffers(FSlateBatchData& BatchData);

	/**
	 * Draws Slate elements
	 *
	 * @param ViewProjectionMatrix	The view projection matrix to pass to the vertex shader
	 * @param RenderBatches			A list of batches that should be rendered.
	 */
	void DrawElements( const FMatrix& ViewProjectionMatrix, const TArray<FSlateRenderBatch>& RenderBatches );

	virtual TSharedRef<FSlateShaderResourceManager> GetResourceManager() override;

	/**
	 * Returns the font cache used when the OpenGL rendering policy is active
	 */
	virtual TSharedRef<FSlateFontCache> GetFontCache() override { return FontCache.ToSharedRef(); }

	virtual bool IsVertexColorInLinearSpace() const override { return false; }

	/** 
	 * Initializes resources if needed
	 */
	void ConditionalInitializeResources();

	/** 
	 * Releases rendering resources
	 */
	void ReleaseResources();
private:
	/** Vertex shader used for all elements */
	FSlateOpenGLVS VertexShader;
	/** Pixel shader used for all elements */
	FSlateOpenGLPS	PixelShader;
	/** Shader program for all elements */
	FSlateOpenGLElementProgram ElementProgram;
	/** Vertex buffer containing all the vertices of every element */
	FSlateOpenGLVertexBuffer VertexBuffer;
	/** Index buffer for accessing vertices of elements */
	FSlateOpenGLIndexBuffer IndexBuffer;
	/** A default white texture to use if no other texture can be found */
	FSlateOpenGLTexture* WhiteTexture;
	/** The font cache for accessing text rendering data */
	TSharedPtr<FSlateFontCache> FontCache;
	/** Texture manager for accessing OpenGL textures */
	TSharedPtr<FSlateOpenGLTextureManager> TextureManager;
	/** True if the rendering policy has been initialized */
	bool bIsInitialized;
};

