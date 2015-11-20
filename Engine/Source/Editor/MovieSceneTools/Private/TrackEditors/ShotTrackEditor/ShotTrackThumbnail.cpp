// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "Runtime/Engine/Public/Slate/SlateTextures.h"


namespace ShotTrackThumbnailConstants
{
	const double ThumbnailFadeInDuration = 0.25f;
}


/* FShotTrackThumbnail structors
 *****************************************************************************/

FShotTrackThumbnail::FShotTrackThumbnail(TSharedPtr<FShotSequencerSection> InSection, const FIntPoint& InSize, TRange<float> InTimeRange)
	: OwningSection(InSection)
	, Size(InSize)
	, Texture(nullptr)
	, TimeRange(InTimeRange)
	, FadeInCurve(0.0f, ShotTrackThumbnailConstants::ThumbnailFadeInDuration)
{
	Texture = new FSlateTexture2DRHIRef(GetSize().X, GetSize().Y, PF_B8G8R8A8, nullptr, TexCreate_Dynamic, true);

	BeginInitResource( Texture );

}


FShotTrackThumbnail::~FShotTrackThumbnail()
{
	BeginReleaseResource(Texture);
	FlushRenderingCommands();

	if (Texture) 
	{
		delete Texture;
	}
}


/* FShotTrackThumbnail interface
 *****************************************************************************/

void FShotTrackThumbnail::CopyTextureIn(FSlateRenderTargetRHI* InTexture)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(ReadTexture,
		FSlateRenderTargetRHI*, RenderTarget, InTexture,
		FSlateTexture2DRHIRef*, TargetTexture, Texture,
	{
		RHICmdList.CopyToResolveTarget(RenderTarget->GetRHIRef(), TargetTexture->GetTypedResource(), false, FResolveParams());
	});
}


void FShotTrackThumbnail::DrawThumbnail()
{
	OwningSection.Pin()->DrawViewportThumbnail(SharedThis(this));
	FadeInCurve.PlayReverse( OwningSection.Pin()->GetSequencerWidget() );
}


float FShotTrackThumbnail::GetFadeInCurve() const 
{
	return FadeInCurve.GetLerp();
}


float FShotTrackThumbnail::GetTime() const
{
	return TimeRange.GetLowerBoundValue();
}


bool FShotTrackThumbnail::IsValid() const
{
	return OwningSection.IsValid();
}


bool FShotTrackThumbnail::IsVisible() const
{
	return OwningSection.IsValid()
		? TimeRange.Overlaps(OwningSection.Pin()->GetVisibleTimeRange())
		: false;
}


/* ISlateViewport interface
 *****************************************************************************/

FIntPoint FShotTrackThumbnail::GetSize() const
{
	return Size;
}


FSlateShaderResource* FShotTrackThumbnail::GetViewportRenderTargetTexture() const
{
	return Texture;
}


bool FShotTrackThumbnail::RequiresVsync() const 
{
	return false;
}
