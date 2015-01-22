// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Interface for video tracks details.
 */
class IMediaTrackVideoDetails
{
public:

	/**
	 * Gets the average data rate of the video track in bits per second.
	 *
	 * @return The data rate.
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
	 */
	virtual float GetFrameRate() const = 0;

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
	virtual ~IMediaTrackVideoDetails() { }
};
