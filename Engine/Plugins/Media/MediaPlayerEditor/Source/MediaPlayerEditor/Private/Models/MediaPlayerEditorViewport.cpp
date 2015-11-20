// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MediaPlayerEditorPrivatePCH.h"
#include "IMediaVideoTrack.h"


/* FMediaPlayerEditorViewport structors
*****************************************************************************/

FMediaPlayerEditorViewport::FMediaPlayerEditorViewport()
: EditorTexture(nullptr)
, SlateTexture(nullptr)
{ }


FMediaPlayerEditorViewport::~FMediaPlayerEditorViewport()
{
	ReleaseResources();
}


/* FMediaPlayerEditorViewport interface
*****************************************************************************/

void FMediaPlayerEditorViewport::Initialize(const IMediaVideoTrackPtr& VideoTrack)
{
	ReleaseResources();

	if (VideoTrack.IsValid())
	{
		const bool bCreateEmptyTexture = true;
		const FIntPoint VideoDimensions = VideoTrack->GetDimensions();

		SlateTexture = new FSlateTexture2DRHIRef(VideoDimensions.X, VideoDimensions.Y, PF_B8G8R8A8, nullptr, TexCreate_Dynamic | TexCreate_RenderTargetable, bCreateEmptyTexture);
		EditorTexture = new FMediaPlayerEditorTexture(SlateTexture, VideoTrack.ToSharedRef());

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			RegisterMediaPlayerEditorTexture,
			FMediaPlayerEditorTexture*, EditorTextureParam, EditorTexture,
			{
				EditorTextureParam->Register();
			}
		);
	}
}


/* ISlateViewport interface
*****************************************************************************/

FIntPoint FMediaPlayerEditorViewport::GetSize() const
{
	return (SlateTexture != nullptr)
		? FIntPoint(SlateTexture->GetWidth(), SlateTexture->GetHeight())
		: FIntPoint();
}


class FSlateShaderResource* FMediaPlayerEditorViewport::GetViewportRenderTargetTexture() const
{
	return SlateTexture;
}


bool FMediaPlayerEditorViewport::RequiresVsync() const
{
	return false;
}


/* FMediaPlayerEditorViewport implementation
*****************************************************************************/

void FMediaPlayerEditorViewport::ReleaseResources()
{
	if (SlateTexture != nullptr)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			ReleaseMediaPlayerEditorResources,
			FMediaPlayerEditorTexture*, EditorTextureParam, EditorTexture,
			FSlateTexture2DRHIRef*, SlateTextureParam, SlateTexture,
			{
				EditorTextureParam->Unregister();
				delete EditorTextureParam;

				SlateTextureParam->ReleaseResource();
				delete SlateTextureParam;
			}
		);

		EditorTexture = nullptr;
		SlateTexture = nullptr;
	}
}
