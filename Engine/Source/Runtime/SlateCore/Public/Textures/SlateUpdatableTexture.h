// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FSlateShaderResource;
class FRenderResource;

/**
 * An interface to deal with a slate texture that can be updated dynamically
 */
class FSlateUpdatableTexture
{
public:
	/** Virtual destructor */
	virtual ~FSlateUpdatableTexture() {}

	/**
	 * Gets the interface to the underlying platform independent texture
	 *
	 * @return	FSlateShaderResource*	pointer to the shader resource
	 */
	virtual FSlateShaderResource* GetSlateResource() = 0;

	/**
	 * Gets the interface to the underlying render resource (may not always be used)
	 *
	 * @return	FRenderResource*	pointer to the render resource
	 */
	virtual FRenderResource* GetRenderResource() {return nullptr;}

	/**
	 * Resize the texture.
	 *
	 * @param Width New texture width
	 * @param Height New texture height
	 */
	virtual void ResizeTexture( uint32 Width, uint32 Height ) = 0;

	/**
	 * Updates the texture contents via a byte array. 
	 * Note: This method is not thread safe so make sure you do not use the Bytes data on another after it is passed in
	 *
	 * @param Bytes Array of texture data
	 */
	virtual void UpdateTexture(const TArray<uint8>& Bytes) = 0;

	/**
	 * Updates the texture contents via a byte array making a copy first for thread safety
	 *
	 * @param Bytes Array of texture data
	 */
	virtual void UpdateTextureThreadSafe(const TArray<uint8>& Bytes) = 0;
};
