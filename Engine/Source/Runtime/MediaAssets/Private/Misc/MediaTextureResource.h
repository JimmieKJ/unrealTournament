// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Containers/TripleBuffer.h"
#include "UnrealClient.h"
#include "TextureResource.h"
#include "Containers/Queue.h"

class UMediaTexture;
enum class EMediaTextureSinkFormat;
enum class EMediaTextureSinkMode;

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
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize() const
	{
		return GetResourceSizeBytes();
	}

	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const
	{
		CumulativeResourceSize.AddUnknownMemoryBytes(CachedResourceSizeBytes);
	}

	SIZE_T GetResourceSizeBytes() const
	{
		return CachedResourceSizeBytes;
	}

	/**
	 * Get the render target texture to render to.
	 *
	 * @return The texture, or nullptr if called in Buffered mode or not on rendering thread.
	 */
	FRHITexture* GetTexture();

	/**
	 * Initialize the render target buffer(s).
	 *
	 * @param OutputDim Width and height of the video output (in pixels).
	 * @param BufferDim Width and height of the sink buffer(s) (in pixels).
	 * @param Format The pixel format of the sink's render target texture.
	 * @param Mode The mode to operate the sink in (buffered vs. unbuffered).
	 */
	void InitializeBuffer(FIntPoint OutputDim, FIntPoint BufferDim, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode);

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

	/** Cache the size of this resource, so it doesn't need to be recalculated. */
	void CacheResourceSize();

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
	 * @param OutputDim Width and height of the output texture (in pixels).
	 * @param BufferDim Width and height of the buffer texture(s) (in pixels).
	 * @param Format The new texture format.
	 * @param Mode The new sink mode.
	 */
	void InitializeResource(FIntPoint OutputDim, FIntPoint BufferDim, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode);

	/** Process any queued up tasks on the render thread. */
	void ProcessRenderThreadTasks();

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

	/** The clear color to use. */
	FLinearColor BufferClearColor;

	/**
	 * Width and height of the buffer resources (in pixels).
	 *
	 * This may be less than OutputDimensions, because the buffers can
	 * have formats with multiple pixels packed into a single RGBA tuple.
	 */
	FIntPoint BufferDimensions;

	/** Number of bytes per row in buffer resources. */
	SIZE_T BufferPitch;

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

	/** Total size of this resource.*/
	SIZE_T CachedResourceSizeBytes;

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

	/**
	 * The sink's current state.
	 *
	 * The purpose of this field is to prevent the decoder thread from doing
	 * stupid things, such as calling InitializeBuffer or ShutdownBuffer in
	 * the wrong order. Strictly speaking, it is not thread-safe, because
	 * state changing operations are not atomic in the current implementation.
	 * However, it shouldn't be an issue, because relevant accesses are gated.
	 *
	 * State changes on the decoder thread will be visible there immediately.
	 * There are a couple places where the state is set on the render thread,
	 * but here we are ok with eventual consistency on the decoder thread.
	 * The latest state will eventually trickle down, and in the meantime the
	 * decoder is in a pending state, such as Initializing or ShuttingDown.
	 */
	enum class EState
	{
		Initializing,
		Initialized,
		ShuttingDown,
		ShutDown
	}
	State;
};
