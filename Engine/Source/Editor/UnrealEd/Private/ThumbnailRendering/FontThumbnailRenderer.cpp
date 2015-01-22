// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealEd.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"

UFontThumbnailRenderer::UFontThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFontThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	UFont* const Font = Cast<UFont>(Object);

	if(Font)
	{
		switch(Font->FontCacheType)
		{
		case EFontCacheType::Offline:
			if(Font->Textures.Num() > 0 && Font->Textures[0])
			{
				// Get the texture interface for the font text
				UTexture2D* const Tex = Font->Textures[0];
				OutWidth = FMath::TruncToInt(Zoom * (float)Tex->GetSurfaceWidth());
				OutHeight = FMath::TruncToInt(Zoom * (float)Tex->GetSurfaceHeight());
				return;
			}
			break;

		case EFontCacheType::Runtime:
			if(Font->CompositeFont.DefaultTypeface.Fonts.Num() > 0)
			{
				OutWidth = OutHeight = Zoom * 256.0f;
			}
			break;

		default:
			break;
		};
	}

	OutWidth = OutHeight = 0;
}

void UFontThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas)
{
	UFont* const Font = Cast<UFont>(Object);

	if(Font)
	{
		switch(Font->FontCacheType)
		{
		case EFontCacheType::Offline:
			if(Font->Textures.Num() > 0 && Font->Textures[0])
			{
				FCanvasTileItem TileItem(FVector2D(X, Y), Font->Textures[0]->Resource, FLinearColor::White);
				TileItem.BlendMode = (Font->ImportOptions.bUseDistanceFieldAlpha) ? SE_BLEND_TranslucentDistanceField : SE_BLEND_Translucent;
				Canvas->DrawItem(TileItem);
			}
			break;

		case EFontCacheType::Runtime:
			if(Font->CompositeFont.DefaultTypeface.Fonts.Num() > 0)
			{
				static const int32 FontSize = 28;
				static const float FontScale = 1.0f;

				const FText FontNameText = FText::FromName(Object->GetFName());
				TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

				// Draw the object name using each of the default fonts
				FVector2D CurPos(X, Y);
				for(const FTypefaceEntry& TypefaceEntry : Font->CompositeFont.DefaultTypeface.Fonts)
				{
					FSlateFontInfo FontInfo(Font, FontSize, TypefaceEntry.Name);

					FCharacterList& CharacterList = FontCache->GetCharacterList(FontInfo, FontScale);
					const int32 MaxCharHeight = CharacterList.GetMaxHeight();

					FCanvasTextItem TextItem(CurPos, FontNameText, FontInfo, FLinearColor::White);
					Canvas->DrawItem(TextItem);

					CurPos.Y += MaxCharHeight;
				}
			}
			break;

		default:
			break;
		};
	}
}
