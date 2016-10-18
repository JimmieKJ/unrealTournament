// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TextureResource.h"
#include "TickableObjectRenderThread.h"
#include "UnrealClient.h"


class UMediaTexture;


/**
 * Texture resource type for media textures.
 */
class FMediaTextureResource
	: public FRenderTarget
	, public FTextureResource
	, public FTickerObjectBase
{
	struct FResource
	{
		void* LockedData;
		TRefCountPtr<FRHITexture2D> RenderTarget;
		TRefCountPtr<FRHITexture2D> ShaderResource;

		FResource() : LockedData(nullptr) { }
	};

public:

	/** 
	 * Creates and initializes a new instance.
	 *
	 * @param InOwner The Movie texture object to create a resource for (must not be nullptr).
	 * @param InClearColor The clear color to use.
	 * @param InOutputDimensions
	 * @param InSinkFormat
	 * @param InSinkMode
	 */
	FMediaTextureResource(UMediaTexture& InOwner, const FLinearColor& InClearColor, const FIntPoint& InOutputDimensions, EMediaTextureSinkFormat InSinkFormat, EMediaTextureSinkMode InSinkMode);

	/** Virtual destructor. */
	virtual ~FMediaTextureResource();

public:

	/**
	 * Acquire a pointer to locked texture memory buffer to render to.
	 *
	 * @return Pointer to buffer on success, nullptr otherwise.
	 */
	void* AcquireBuffer();

	/** Display the latest texture buffer. */
	void DisplayBuffer();

	/**
	 * Get the memory size of this resource.
	 *
	 * @return Resource size (in bytes).
	 */
	SIZE_T GetResourceSize() const;

	/**
	 * Get the render target texture to render to.
	 *
	 * @return The texture, or nullptr if called in Buffered mode or not on rendering thread.
	 */
	FRHITexture* GetTexture();

	/**
	 * Initialize the render target buffer(s).
	 *
	 * @param Dimensions Width and height of the texture (in pixels).
	 * @param Format The pixel format of the sink's render target texture.
	 * @param Mode The mode to operate the sink in (buffered vs. unbuffered).
	 */
	void InitializeBuffer(FIntPoint Dimensions, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode);

	/** Release a previously acquired texture buffer. */
	void ReleaseBuffer();

	/** Shut down the render target buffers. */
	void ShutdownBuffer();

	/**
	 * Update the render target texture with new frame data.
	 *
	 * @param Data The frame data to copy into the render target.
	 * @param Pitch Number of bytes per pixel row (0 = default).
	 */
	void UpdateBuffer(const uint8* Data, uint32 Pitch);

	/**
	 * Update the underlying resource textures.
	 *
	 * @param RenderTarget The new render target texture.
	 * @param ShaderResource The new shader resource texture.
	 */
	void UpdateTextures(FRHITexture* RenderTarget, FRHITexture* ShaderResource);

public:

	//~ FRenderTarget interface

	virtual FIntPoint GetSizeXY() const override;

public:

	//~ FTextureResource interface

	virtual FString GetFriendlyName() const override;
	virtual uint32 GetSizeX() const override;
	virtual uint32 GetSizeY() const override;
	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

public:

	//~ FTickerObjectBase interface

	virtual bool Tick(float DeltaTime) override;

protected:

	/**
	 * Helper function to convert the given resource to the target resource's pixel format.
	 *
	 * @param Resource The texture resource to convert.
	 * @see DisplayResource
	 */
	void ConvertResource(const FResource& Resource);

	/**
	 * Helper function to display the given resource in the media texture.
	 *
	 * @param Resource The resource to display.
	 * @see ConvertResource
	 */
	void DisplayResource(const FResource& Resource);

	/**
	 * Initialize this resource.
	 *
	 * @param Dimensions The new texture dimensions.
	 * @param Format The new texture format.
	 * @param Mode The new sink mode.
	 */
	void InitializeResource(FIntPoint Dimensions, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode);

	/**
	 * Helper function to make the given resource the output.
	 *
	 * @param Resource The resource to set.
	 */
	void SetResource(const FResource& Resource);

	/** Swap the render target resource for display. */
	void SwapResource();

private:

	/** The media texture that owns this resource. */
	UMediaTexture& Owner;

	/** Triple-buffer for texture resources. */
	TTripleBuffer<FResource> TripleBuffer;

private:

	//~ The following fields are owned by the render thread

	/** Number of bytes per pixel in buffer resources. */
	uint8 BufferBytesPerPixel;

	/** The clear color to use. */
	FLinearColor BufferClearColor;

	/**
	 * Width and height of the buffer resources (in pixels).
	 *
	 * This may be less than OutputDimensions, because the buffers can
	 * have formats with multiple pixels packed into a single RGBA tuple.
	 */
	FIntPoint BufferDimensions;

	/**
	 * Texture resources for buffered mode or pixel conversions.
	 *
	 * In Buffered mode, all three resources are used for triple buffering.
	 * If no pixel format conversion is required, the triple buffer's Read
	 * buffer is used as the output resource. Otherwise the Read buffer is
	 * converted into a separate output resource.
	 *
	 * In Unbuffered mode when pixel format conversion is required, only the
	 * third buffer (index = 2) is used. Otherwise data is written directly
	 * to a separate output resource.
	 */
	FResource BufferResources[3];

	/** Width and height of the output resource (in pixels). */
	FIntPoint OutputDimensions;

	/**
	 * Asynchronous tasks for the render thread.
	 *
	 * We can't reliably enqueue render commands using the existing macros
	 * from the decoder thread, which runs neither on the game nor on the
	 * render thread, so we maintain our own queue of render commands here.
	 */
	TQueue<TFunction<void()>> RenderThreadTasks;

	/** Whether the current media sink format requires pixel format conversion. */
	bool RequiresConversion;

	/** The render target's pixel format. */
	EMediaTextureSinkFormat SinkFormat;

	/** The mode that this sink is currently operating in. */
	EMediaTextureSinkMode SinkMode;

private:

	//~ The following fields are owned by the decoder thread

	/** The resource's current state. */
	enum class EState
	{
		Initializing,
		Initialized,
		ShuttingDown,
		ShutDown
	}
	State;
};
