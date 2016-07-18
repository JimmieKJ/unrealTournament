//! @file SubstanceTextureThumbnailRenderer.cpp
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.

#include "SubstanceEditorPrivatePCH.h"
#include "SubstanceCoreTypedefs.h"
#include "SubstanceCoreHelpers.h"

#include "ImageUtils.h"


USubstanceTextureThumbnailRenderer::USubstanceTextureThumbnailRenderer(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
}

void USubstanceTextureThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas)
{
	USubstanceTexture2D* Texture = Cast<USubstanceTexture2D>(Object);

	// special case for orphan substance textures, give some feedback to the user
	if (Texture && Texture->ParentInstance == NULL)
	{
		UFont* Font = GEngine->GetLargeFont();

		Super::Draw(Object, X, Y, Width, Height, RenderTarget, Canvas);

		Canvas->DrawShadowedString(
				10.0f, Height * 0.4f,
				TEXT("Substance Texture is missing"),
				Font, FColor(200,50,50));

		Canvas->DrawShadowedString(
			10.0f, Height * 0.45f,
			TEXT("its Graph Instance"),
			Font, FColor(200, 50, 50));
	}
	// special case for orphan substance textures, give some feedback to the user
	else if (Texture && Texture->ParentInstance &&  Texture->ParentInstance->Parent == NULL)
	{
		UFont* Font = GEngine->GetLargeFont();

		Super::Draw(Object, X, Y, Width, Height, RenderTarget, Canvas);

		Canvas->DrawShadowedString(
			10.0f, Height * 0.4f,
			TEXT("Substance Texture's Graph Instance"),
			Font, FColor(200, 50, 50));

		Canvas->DrawShadowedString(
			10.0f, Height * 0.45f,
			TEXT(" is missing its Instance Factory"),
			Font, FColor(200, 50, 50));
	}
	else
	{
		Super::Draw(Object, X, Y, Width, Height, RenderTarget, Canvas);
	}
}
