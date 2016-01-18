// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class IMediaStream;


/**
 * Interface for video tracks.
 */
class IMediaVideoTrack
{
public:

	/**
	 * Gets the average data rate of the video track in bits per second.
	 *
	 * @return The data rate.
	 * @see GetFrameRate
	 */
	virtual uint32 GetBitRate() const = 0;

	/**
	 * Gets the video's dimensions in pixels.
	 *
	 * @return The width and height of the video.
	 * @see GetAspectRatio
	 */
	virtual FIntPoint GetDimensions() const = 0;

	/**
	 * Gets the video's frame rate in frames per second.
	 *
	 * @return The frame rate.
	 * @see GetBitRate
	 */
	virtual float GetFrameRate() const = 0;

	/**
	 * Get the underlying media stream.
	 *
	 * @return Media stream object.
	 */
	virtual IMediaStream& GetStream() = 0;

#if WITH_ENGINE
	/**
	 * Bind the given texture resource to the track.
	 *
	 * Use this method to bypass the slower CPU-copied deferred media sinks on
	 * media streams and directly update the resource with the GPU if desired.
	 *
	 * @param Texture The texture to bind.
	 * @see UnbindTexture
	 */
	virtual void BindTexture(class FRHITexture* Texture) = 0;

	/**
	 * Unbind the given texture resource from the track.
	 *
	 * @param Texture The texture to unbind.
	 * @see BindTexture
	 */
	virtual void UnbindTexture(class FRHITexture* Texture) = 0;
#endif

public:

	/**
	 * Gets the media's aspect ratio.
	 *
	 * @return Aspect ratio.
	 * @see GetDimensions
	 */
	float GetAspectRatio() const
	{
		const FVector2D Dimensions = GetDimensions();

		if (FMath::IsNearlyZero(Dimensions.Y))
		{
			return 0.0f;
		}

		return Dimensions.X / Dimensions.Y;
	}

public:

	/** Virtual destructor. */
	~IMediaVideoTrack() { }
};


/** Type definition for shared pointers to instances of IMediaVideoTrack. */
typedef TSharedPtr<IMediaVideoTrack, ESPMode::ThreadSafe> IMediaVideoTrackPtr;

/** Type definition for shared references to instances of IMediaVideoTrack. */
typedef TSharedRef<IMediaVideoTrack, ESPMode::ThreadSafe> IMediaVideoTrackRef;
