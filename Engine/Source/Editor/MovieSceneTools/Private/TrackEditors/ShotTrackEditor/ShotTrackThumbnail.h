// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FShotSequencerSection;
class FSlateRenderTargetRHI;
class FSlateTexture2DRHIRef;


/**
 * Shot Thumbnail, which keeps a Texture to be displayed by a viewport.
 */
class FShotTrackThumbnail
	: public ISlateViewport
	, public TSharedFromThis<FShotTrackThumbnail>
{
public:

	/** Create and initialize a new instance. */
	FShotTrackThumbnail(TSharedPtr<FShotSequencerSection> InSection, const FIntPoint& InSize, TRange<float> InTimeRange);

	/** Virtual destructor. */
	virtual ~FShotTrackThumbnail();

public:

	/** Copies the incoming render target to this thumbnails texture. */
	void CopyTextureIn(FSlateRenderTargetRHI* InTexture);

	/** Renders the thumbnail to the texture. */
	void DrawThumbnail();

	/** Gets the curve for fading in the thumbnail. */
	float GetFadeInCurve() const;

	/** Gets the time that this thumbnail is a rendering of. */
	float GetTime() const;

	bool IsValid() const;

	/** Returns whether this thumbnail is visible based on the shot section geometry visibility. */
	bool IsVisible() const;

public:

	// ISlateViewport interface

	virtual FIntPoint GetSize() const override;
	virtual FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual bool RequiresVsync() const override;

private:

	/** Parent shot section we are a thumbnail of. */
	TWeakPtr<FShotSequencerSection> OwningSection;
	
	FIntPoint Size;

	/** The Texture RHI that holds the thumbnail. */
	FSlateTexture2DRHIRef* Texture;

	/** Where in time this thumbnail is a rendering of. */
	TRange<float> TimeRange;

	/** Fade curve to display while the thumbnail is redrawing. */
	FCurveSequence FadeInCurve;
};
