// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FRHITexture;
class FRHITexture2D;


/**
 * Available pixel formats for media texture sinks.
 */
enum class EMediaTextureSinkFormat
{
	/** Four 8-bit unsigned integer components (AYUV packing) per pixel. */
	CharAYUV,

	/** Four 8-bit unsigned integer components (Blue, Green, Red, Alpha) per pixel. */
	CharBGRA,

	/** NV12 encoded monochrome texture with 8 bits per channel. */
	CharNV12,

	/** NV21 encoded monochrome texture with 8 bits per channel. */
	CharNV21,

	/** Four 8-bit unsigned integer components (UYVY packing) per pixel. */
	CharUYVY,

	/** Four 8-bit unsigned integer components (YUY2 packing) per pixel. */
	CharYUY2,

	/** Four 8-bit unsigned integer components (YVYU packing) per pixel. */
	CharYVYU,

	/** Three floating point components (Red, Green, Blue) per pixel. */
	FloatRGB,

	/** Four floating point components (Red, Green, Blue, Alpha) per pixel. */
	FloatRGBA,
};


/**
 * Available modes that texture sinks can operate in.
 */
enum class EMediaTextureSinkMode
{
	/** Use triple-buffered texture memory (for decoding on non-render threads). */
	Buffered,

	/**
	 * Render to the render target's texture resource.
	 *
	 * Use IMediaTextureSink.GetTextureSinkTexture to get the sink's render target
	 * texture resource. This mode is only available when compiling against the RHI.
	 */
	Unbuffered
};


/**
 * Interface for media sinks that receive texture data.
 *
 * Implementors of this interface must be thread-safe as the methods may be called
 * from any random media decoder thread.
 *
 * Texture sinks can operate in one of two modes: Unbuffered and Buffered. Unbuffered
 * mode provides direct access to the texture resource, and it is the preferred method
 * if the native media player is able to decode or return the decoded frame buffer on
 * the render thread, or if it performs custom pixel conversions on the render thread.
 *
 * Unbuffered mode also allows for replacing the underlying resources directly. This
 * is the fastest mechanism, but generally only useful for decoders that can decode
 * directly on the GPU and hence provide their own texture resources.
 *
 * Note: Unbuffered mode is only exposed in the API when compiling against the Engine,
 * because texture resources are Engine RHI types.
 *
 * Buffered mode provides access to low-level untyped texture memory in one or more
 * intermediate buffers. It can be used in applications that are not compiled against
 * the Engine, or in native media players that return decoded frame data on their own
 * thread. It is possible to either decode directly into the buffers using the
 * AcquireTextureSinkBuffer and ReleaseTextureSinkBuffer methods, or to copy an
 * existing buffer using the UpdateTextureSinkBuffer method. Because buffered mode
 * uses intermediate buffers to decouple the decoder from the render thread, it may
 * use considerably more memory and CPU time.
 *
 * Depending on when and how the video frames are being made available by the decoder,
 * native media players should chose one of the following four mechanisms to send new
 * video frame data to the texture sinks:
 *
 * - GetTextureSinkTexture() [implies Unbuffered mode]
 * - AcquireTextureSinkBuffer() + ReleaseTextureSinkBuffer() + DisplayTextureSinkBuffer()
 * - UpdateTextureSinkBuffer()
 * - UpdateTextureSinkResource()
 *
 * Texture buffers are initialized with one of the pixel formats in EMediaTextureSinkMode.
 * If a texture format is not supported natively by a sink, it must perform any required
 * pixel format conversion, which may add a cost on CPU time, GPU time, or both.
 *
 * Note: Most common video formats are not supported natively. Most decoders can output
 * frames in a supported format, such as RGB or RGBA. However, it may still be a lot
 * faster to have the sink perform the pixel format conversion on the GPU instead,
 * because decoders often use a much slower conversion on the CPU.
 */
class IMediaTextureSink
{
public:

	/**
	 * Get the current dimensions of the texture sink's frame buffer.
	 *
	 * @return Width and height of the buffer (in pixels).
	 * @see GetTextureSinkFormat, GetTextureSinkMode, InitializeTextureSink
	 */
	virtual FIntPoint GetTextureSinkDimensions() const = 0;

	/**
	 * Get the sink's current texture format.
	 *
	 * @return texture format.
	 * @see GetTextureSinkDimensions, GetTextureSinkMode, InitializeTextureSink
	 */
	virtual EMediaTextureSinkFormat GetTextureSinkFormat() const = 0;

	/**
	 * Get the texture sink's current buffer mode.
	 *
	 * @return Buffer mode.
	 * @see GetTextureSinkDimensions, GetTextureSinkFormat, InitializeTextureSink
	 */
	virtual EMediaTextureSinkMode GetTextureSinkMode() const = 0;

	/**
	 * Initialize the sink.
	 *
	 * @param Dimensions Width and height of the texture (in pixels).
	 * @param Format The pixel format of the sink's render target texture.
	 * @param Mode The mode to operate the sink in (buffered vs. unbuffered).
	 * @return true on success, false if initialization is already in progress or failed.
	 * @see GetTextureSinkDimensions, GetTextureSinkMode, ShutdownSink, SupportsTextureSinkFormat
	 */
	virtual bool InitializeTextureSink(FIntPoint Dimensions, EMediaTextureSinkFormat Format, EMediaTextureSinkMode Mode) = 0;

	/**
	 * Shut down the sink.
	 *
	 * @see InitializeSink
	 */
	virtual void ShutdownTextureSink() = 0;

	/**
	 * Whether the sink supports the specified pixel format.
	 *
	 * @param Format The format to check.
	 * @return true if the format is supported, false otherwise.
	 * @see InitializeTextureSink
	 */
	virtual bool SupportsTextureSinkFormat(EMediaTextureSinkFormat Format) const = 0;

public:

	//~ Method 1: Get the sink's render target texture and write to it

#if WITH_ENGINE

	/**
	 * Get the render target texture to render to.
	 *
	 * If called on the rendering thread and when the sink has been initialized in
	 * Unbuffered mode, this method returns a texture object that can be written to
	 * directly on the render thread.
	 *
	 * @return The texture, or nullptr if called in Buffered mode or not on rendering thread.
	 * @see InitializeTextureSink
	 */
	virtual FRHITexture* GetTextureSinkTexture() = 0;

#endif

public:

	//~ Method 2: Get the render target's memory and write, release, and display it

	/**
	 * Acquire a pointer to locked texture memory buffer to render to.
	 *
	 * This method can be used to acquire a render target texture buffer that
	 * the decoder can decode a frame into. In Unbuffered mode it must be called
	 * on the render thread to have an effect. In Buffered mode it can be called
	 * from on thread, but the sink may return an intermediate buffer that will
	 * be copied when released.
	 *
	 * If a valid pointer is returned by this method, the caller must call
	 * ReleaseTextureSinkBuffer to release the buffer. In Unbuffered mode,
	 * the changes are committed immediately when the buffer is released.
	 * In Buffered mode the caller must call DisplayTextureSinkBuffer to
	 * commit the buffer to the GPU.
	 *
	 * If the decoder provides its own frame buffer, consider using the
	 * UpdateTextureSinkBuffer instead.
	 *
	 * @return Pointer to buffer on success, nullptr otherwise.
	 * @see DisplayTextureSinkBuffer, InitializeTextureSink, ReleaseTextureSinkBuffer
	 */
	virtual void* AcquireTextureSinkBuffer() = 0;

	/**
	 * Display the latest texture buffer.
	 *
	 * This method is used only if the sink has been initialized in Buffered
	 * mode. It signals the sink that the previously released or updated texture
	 * buffer is ready for display.
	 *
	 * @param Time The time of the frame relative to the beginning of the media.
	 * @see AcquireTextureSinkBuffer, ReleaseTextureSinkBuffer, UpdateTextureSinkBuffer
	 */
	virtual void DisplayTextureSinkBuffer(FTimespan Time) = 0;

	/**
	 * Release a previously acquired texture buffer.
	 *
	 * This method is used to release a previously acquired render target texture
	 * buffer from the sink. It indicates to the sink that writing to the texture
	 * buffer has completed. In Unbuffered mode, the changes are committed
	 * immediately. In Buffered mode the caller must call DisplayTextureSinkBuffer
	 * to commit the buffer to the GPU.
	 *
	 * @see AcquireTextureSinkBuffer, DisplayTextureSinkBuffer
	 */
	virtual void ReleaseTextureSinkBuffer() = 0;

public:

	//~ Method 3: Update the render target texture from a provided buffer

	/**
	 * Update the render target texture with new frame data.
	 *
	 * This method can be used to update the sink's render target texture with data
	 * from a frame buffer that was provided by the decoder. In Unbuffered mode it
	 * must be called on the render thread to have an effect. In Buffered mode it
	 * can be called on any thread, but the sink may perform extra buffer copies.
	 *
	 * In Unbuffered mode, the changes are committed immediately. In Buffered mode
	 * the caller must call DisplayTextureSinkBuffer to commit the buffer to the GPU.
	 *
	 * If the decoder does not provide its own frame buffer, consider using the
	 * AcquireTextureSinkBuffer and ReleaseTextureSinkBuffer instead.
	 *
	 * @param Data The frame data to copy into the render target.
	 * @param Pitch Number of bytes per pixel row (0 = default).
	 * @see DisplayTextureSinkBuffer, InitializeTextureSink
	 */
	virtual void UpdateTextureSinkBuffer(const uint8* Data, uint32 Pitch = 0) = 0;

public:

	//~ Method 4: Replace the sink's texture resource with your own

#if WITH_ENGINE

	/**
	 * Update the sink's RHI resources.
	 *
	 * This method can be used to replace the sink's underlying texture resources
	 * with different textures, which is the recommended way if the decoder provides
	 * its own render targets. It can be called from any thread.
	 *
	 * This method implies Unbuffered mode and does not require any buffer copies,
	 * but the sink may perform a pixel format conversion if the format is not
	 * natively supported by the renderer.
	 *
	 * @param RenderTarget The new render target texture.
	 * @param ShaderResource The new shader resource texture.
	 */
	virtual void UpdateTextureSinkResource(FRHITexture* RenderTarget, FRHITexture* ShaderResource) = 0;

#endif

public:

	/** Virtual destructor. */
	~IMediaTextureSink() { }
};
