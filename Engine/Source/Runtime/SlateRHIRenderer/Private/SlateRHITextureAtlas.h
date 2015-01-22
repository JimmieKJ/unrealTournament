// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Represents a texture atlas for use with RHI.
 */
class FSlateTextureAtlasRHI
	: public FSlateTextureAtlas
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InWidth
	 * @param InHeight
	 * @param PaddingStyle
	 */
	FSlateTextureAtlasRHI( uint32 InWidth, uint32 InHeight, ESlateTextureAtlasPaddingStyle PaddingStyle );

	/**
	 * Destructor.
	 */
	~FSlateTextureAtlasRHI( );

public:

	/**
	 * Gets the atlas' underlying texture resource.
	 *
	 * @return The texture resource.
	 */
	FSlateTexture2DRHIRef* GetAtlasTexture( ) const
	{
		return AtlasTexture;
	}

	/**
	 * Releases rendering resources from the texture.
	 */	
	void ReleaseAtlasTexture( );
	
	/**
	 * Updates the texture on the render thread.
	 *
	 * @param RenderThreadData
	 */
	void UpdateTexture_RenderThread( FSlateTextureData* RenderThreadData );

public:

	// FSlateTextureAtlas overrides.

	virtual void ConditionalUpdateTexture( );

private:

	/** The texture rendering resource */
	FSlateTexture2DRHIRef* AtlasTexture;
};
