// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FMediaPlayerEditorViewport
	: public ISlateViewport
{
public:

	/** Default constructor. */
	FMediaPlayerEditorViewport();

	/** Destructor. */
	~FMediaPlayerEditorViewport();

public:

	/**
	 * Initializes the viewport with the specified video track.
	 *
	 * @param VideoTrack The video track to render to the viewport.
	 */
	void Initialize(const IMediaVideoTrackPtr& VideoTrack);

public:

	// ISlateViewport interface

	virtual FIntPoint GetSize() const override;
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual bool RequiresVsync() const override;

protected:

	/** Releases the resources associated with this viewport. */
	void ReleaseResources();

private:

	/** The texture being rendered on this viewport. */
	FMediaPlayerEditorTexture* EditorTexture;

	/** Pointer to the Slate texture being rendered. */
	FSlateTexture2DRHIRef* SlateTexture;
};