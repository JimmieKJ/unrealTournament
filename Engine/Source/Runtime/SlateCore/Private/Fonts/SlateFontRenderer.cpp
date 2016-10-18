// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "SlateFontRenderer.h"
#include "SlateTextShaper.h"
#include "FontCacheCompositeFont.h"
#include "LegacySlateFontInfoCache.h"


namespace SlateFontRendererUtils
{

#if WITH_FREETYPE

void AppendGlyphFlags(const FFontData& InFontData, uint32& InOutGlyphFlags)
{
	// Setup additional glyph flags
	InOutGlyphFlags |= GlobalGlyphFlags;

	switch(InFontData.Hinting)
	{
	case EFontHinting::Auto:		InOutGlyphFlags |= FT_LOAD_FORCE_AUTOHINT; break;
	case EFontHinting::AutoLight:	InOutGlyphFlags |= FT_LOAD_TARGET_LIGHT; break;
	case EFontHinting::Monochrome:	InOutGlyphFlags |= FT_LOAD_TARGET_MONO | FT_LOAD_FORCE_AUTOHINT; break;
	case EFontHinting::None:		InOutGlyphFlags |= FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING; break;
	case EFontHinting::Default:
	default:						InOutGlyphFlags |= FT_LOAD_TARGET_NORMAL; break;
	}
}

#endif // WITH_FREETYPE

} // namespace SlateFontRendererUtils


FSlateFontRenderer::FSlateFontRenderer(const FFreeTypeLibrary* InFTLibrary, FFreeTypeGlyphCache* InFTGlyphCache, FFreeTypeKerningPairCache* InFTKerningPairCache, FCompositeFontCache* InCompositeFontCache)
	: FTLibrary(InFTLibrary)
	, FTGlyphCache(InFTGlyphCache)
	, FTKerningPairCache(InFTKerningPairCache)
	, CompositeFontCache(InCompositeFontCache)
{
	check(FTLibrary);
	check(FTGlyphCache);
	check(FTKerningPairCache);
	check(CompositeFontCache);
}

uint16 FSlateFontRenderer::GetMaxHeight(const FSlateFontInfo& InFontInfo, const float InScale) const
{
#if WITH_FREETYPE
	// Just get info for the null character 
	TCHAR Char = 0;
	const FFontData& FontData = CompositeFontCache->GetDefaultFontData(InFontInfo);
	const FFreeTypeFaceGlyphData FaceGlyphData = GetFontFaceForCharacter(FontData, Char, InFontInfo.FontFallback);

	if (FaceGlyphData.FaceAndMemory.IsValid())
	{
		FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
		if (FTGlyphCache->FindOrCache(FaceGlyphData.FaceAndMemory->GetFace(), FaceGlyphData.GlyphIndex, FaceGlyphData.GlyphFlags, InFontInfo.Size, InScale, CachedGlyphData))
		{
			return static_cast<uint16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(FT_MulFix(CachedGlyphData.Height, CachedGlyphData.SizeMetrics.y_scale)) * InScale);
		}
	}

	return 0;
#else
	return 0;
#endif // WITH_FREETYPE
}

int16 FSlateFontRenderer::GetBaseline(const FSlateFontInfo& InFontInfo, const float InScale) const
{
#if WITH_FREETYPE
	// Just get info for the null character 
	TCHAR Char = 0;
	const FFontData& FontData = CompositeFontCache->GetDefaultFontData(InFontInfo);
	const FFreeTypeFaceGlyphData FaceGlyphData = GetFontFaceForCharacter(FontData, Char, InFontInfo.FontFallback);

	if (FaceGlyphData.FaceAndMemory.IsValid())
	{
		FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
		if (FTGlyphCache->FindOrCache(FaceGlyphData.FaceAndMemory->GetFace(), FaceGlyphData.GlyphIndex, FaceGlyphData.GlyphFlags, InFontInfo.Size, InScale, CachedGlyphData))
		{
			return static_cast<int16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(CachedGlyphData.SizeMetrics.descender) * InScale);
		}
	}

	return 0;
#else
	return 0;
#endif // WITH_FREETYPE
}

void FSlateFontRenderer::GetUnderlineMetrics(const FSlateFontInfo& InFontInfo, const float InScale, int16& OutUnderlinePos, int16& OutUnderlineThickness) const
{
#if WITH_FREETYPE
	const FFontData& FontData = CompositeFontCache->GetDefaultFontData(InFontInfo);
	FT_Face FontFace = GetFontFace(FontData);

	if (FontFace && FT_IS_SCALABLE(FontFace))
	{
		FreeTypeUtils::ApplySizeAndScale(FontFace, InFontInfo.Size, InScale);

		OutUnderlinePos = static_cast<int16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(FT_MulFix(FontFace->underline_position, FontFace->size->metrics.y_scale)) * InScale);
		OutUnderlineThickness = static_cast<int16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(FT_MulFix(FontFace->underline_thickness, FontFace->size->metrics.y_scale)) * InScale);
	}
	else
#endif // WITH_FREETYPE
	{
		OutUnderlinePos = 0;
		OutUnderlineThickness = 0;
	}
}

bool FSlateFontRenderer::HasKerning(const FFontData& InFontData) const
{
#if WITH_FREETYPE
	FT_Face FontFace = GetFontFace(InFontData);

	if (!FontFace)
	{
		return false;
	}

	return FT_HAS_KERNING(FontFace) != 0;
#else
	return false;
#endif // WITH_FREETYPE
}

int8 FSlateFontRenderer::GetKerning(const FFontData& InFontData, const int32 InSize, TCHAR First, TCHAR Second, const float InScale) const
{
#if WITH_FREETYPE
	int8 Kerning = 0;
	int32* FoundKerning = nullptr;

	FT_Face FontFace = GetFontFace(InFontData);

	// Check if this font has kerning as not all fonts do.
	// We also can't perform kerning between two separate font faces
	if (FontFace && FT_HAS_KERNING(FontFace))
	{
		FT_UInt FirstIndex = FT_Get_Char_Index(FontFace, First);
		FT_UInt SecondIndex = FT_Get_Char_Index(FontFace, Second);

		FT_Vector KerningVec;
		if (FTKerningPairCache->FindOrCache(FontFace, FFreeTypeKerningPairCache::FKerningPair(FirstIndex, SecondIndex), FT_KERNING_DEFAULT, InSize, InScale, KerningVec))
		{
			// Return pixel sizes
			Kerning = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int8>(KerningVec.x);
		}
	}

	return Kerning;
#else
	return 0;
#endif // WITH_FREETYPE
}

bool FSlateFontRenderer::CanLoadCharacter(const FFontData& InFontData, TCHAR Char, EFontFallback MaxFallbackLevel) const
{
	bool bReturnVal = false;

#if WITH_FREETYPE
	const FFreeTypeFaceGlyphData FaceGlyphData = GetFontFaceForCharacter(InFontData, Char, MaxFallbackLevel);
	bReturnVal = FaceGlyphData.FaceAndMemory.IsValid() && FaceGlyphData.GlyphIndex != 0;
#endif // WITH_FREETYPE

	return bReturnVal;
}

#if WITH_FREETYPE

FFreeTypeFaceGlyphData FSlateFontRenderer::GetFontFaceForCharacter(const FFontData& InFontData, TCHAR Char, EFontFallback MaxFallbackLevel) const 
{
	FFreeTypeFaceGlyphData ReturnVal;

	TSharedPtr<FFreeTypeFace> FaceAndMemory = CompositeFontCache->GetFontFace(InFontData);
	uint32 GlyphIndex = 0;
	const bool bOverrideFallback = Char == SlateFontRendererUtils::InvalidSubChar;

	if (FaceAndMemory.IsValid())
	{
		// Get the index to the glyph in the font face
		GlyphIndex = FT_Get_Char_Index(FaceAndMemory->GetFace(), Char);
		ReturnVal.CharFallbackLevel = EFontFallback::FF_NoFallback;
	}

	// If the requested glyph doesn't exist, use the localization fallback font
	if (!FaceAndMemory.IsValid() || (Char != 0 && GlyphIndex == 0))
	{
		const bool bCanFallback = bOverrideFallback || MaxFallbackLevel >= EFontFallback::FF_LocalizedFallback;

		if (bCanFallback)
		{
			FaceAndMemory = CompositeFontCache->GetFontFace(FLegacySlateFontInfoCache::Get().GetLocalizedFallbackFontData());

			if (FaceAndMemory.IsValid())
			{	
				GlyphIndex = FT_Get_Char_Index(FaceAndMemory->GetFace(), Char);

				ReturnVal.CharFallbackLevel = EFontFallback::FF_LocalizedFallback;
				ReturnVal.GlyphFlags |= FT_LOAD_FORCE_AUTOHINT;
			}
		}
	}

	// If the requested glyph doesn't exist, use the last resort fallback font
	if (!FaceAndMemory.IsValid() || (Char != 0 && GlyphIndex == 0))
	{
		bool bCanFallback = bOverrideFallback || MaxFallbackLevel >= EFontFallback::FF_LastResortFallback;

		if (bCanFallback)
		{
			FaceAndMemory = CompositeFontCache->GetFontFace(FLegacySlateFontInfoCache::Get().GetLastResortFontData());

			if (FaceAndMemory.IsValid())
			{
				GlyphIndex = FT_Get_Char_Index(FaceAndMemory->GetFace(), Char);

				ReturnVal.CharFallbackLevel = EFontFallback::FF_LastResortFallback;
				ReturnVal.GlyphFlags |= FT_LOAD_FORCE_AUTOHINT;
			}
		}
	}

	ReturnVal.FaceAndMemory = FaceAndMemory;
	ReturnVal.GlyphIndex = GlyphIndex;

	return ReturnVal;
}

#endif // WITH_FREETYPE

bool FSlateFontRenderer::GetRenderData(const FFontData& InFontData, const int32 InSize, TCHAR Char, FCharacterRenderData& OutRenderData, const float InScale, EFontFallback* OutFallbackLevel) const
{
#if WITH_FREETYPE
	FFreeTypeFaceGlyphData FaceGlyphData = GetFontFaceForCharacter(InFontData, Char, EFontFallback::FF_Max);
	if (FaceGlyphData.FaceAndMemory.IsValid())
	{
		check(FaceGlyphData.FaceAndMemory->IsValid());

		if (OutFallbackLevel)
		{
			*OutFallbackLevel = FaceGlyphData.CharFallbackLevel;
		}

		SlateFontRendererUtils::AppendGlyphFlags(InFontData, FaceGlyphData.GlyphFlags);

		FT_Error Error = FreeTypeUtils::LoadGlyph(FaceGlyphData.FaceAndMemory->GetFace(), FaceGlyphData.GlyphIndex, FaceGlyphData.GlyphFlags, InSize, InScale);
		check(Error == 0);

		OutRenderData.Char = Char;
		return GetRenderData(FaceGlyphData, InScale, OutRenderData);
	}
#endif // WITH_FREETYPE
	return false;
}

bool FSlateFontRenderer::GetRenderData(const FShapedGlyphEntry& InShapedGlyph, FCharacterRenderData& OutRenderData) const
{
#if WITH_FREETYPE
	FFreeTypeFaceGlyphData FaceGlyphData;
	FaceGlyphData.FaceAndMemory = InShapedGlyph.FontFaceData->FontFace.Pin();
	FaceGlyphData.GlyphIndex = InShapedGlyph.GlyphIndex;
	FaceGlyphData.GlyphFlags = InShapedGlyph.FontFaceData->GlyphFlags;
	if (FaceGlyphData.FaceAndMemory.IsValid())
	{
		check(FaceGlyphData.FaceAndMemory->IsValid());

		FT_Error Error = FreeTypeUtils::LoadGlyph(FaceGlyphData.FaceAndMemory->GetFace(), FaceGlyphData.GlyphIndex, FaceGlyphData.GlyphFlags, InShapedGlyph.FontFaceData->FontSize, InShapedGlyph.FontFaceData->FontScale);
		check(Error == 0);

		OutRenderData.Char = 0;
		return GetRenderData(FaceGlyphData, InShapedGlyph.FontFaceData->FontScale, OutRenderData);
	}
#endif // WITH_FREETYPE
	return false;
}

#if WITH_FREETYPE

bool FSlateFontRenderer::GetRenderData(const FFreeTypeFaceGlyphData& InFaceGlyphData, const float InScale, FCharacterRenderData& OutRenderData) const
{
	FT_Face Face = InFaceGlyphData.FaceAndMemory->GetFace();

	// Get the slot for the glyph.  This contains measurement info
	FT_GlyphSlot Slot = Face->glyph;
		
	FT_Render_Glyph(Slot, FT_RENDER_MODE_NORMAL);

	// one byte per pixel 
	const uint32 GlyphPixelSize = 1;

	FT_Bitmap* Bitmap = nullptr;

	FT_Bitmap NewBitmap;
	if (Slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
	{
		FT_Bitmap_New(&NewBitmap);
		// Convert the mono font to 8bbp from 1bpp
		FT_Bitmap_Convert(FTLibrary->GetLibrary(), &Slot->bitmap, &NewBitmap, 4);

		Bitmap = &NewBitmap;
	}
	else
	{
		Bitmap = &Slot->bitmap;
	}
		
	OutRenderData.RawPixels.Reset();
	OutRenderData.RawPixels.AddUninitialized(Bitmap->rows * Bitmap->width);

	// Nothing to do for zero width or height glyphs
	if (OutRenderData.RawPixels.Num())
	{
		// Copy the rendered bitmap to our raw pixels array
		if (Slot->bitmap.pixel_mode != FT_PIXEL_MODE_MONO)
		{
			for (uint32 Row = 0; Row < (uint32)Bitmap->rows; ++Row)
			{
				// Copy a single row. Note Bitmap.pitch contains the offset (in bytes) between rows.  Not always equal to Bitmap.width!
				FMemory::Memcpy(&OutRenderData.RawPixels[Row*Bitmap->width], &Bitmap->buffer[Row*Bitmap->pitch], Bitmap->width*GlyphPixelSize);
			}

		}
		else
		{
			// In Mono a value of 1 means the pixel is drawn and a value of zero means it is not. 
			// So we must check each pixel and convert it to a color.
			for (uint32 Height = 0; Height < (uint32)Bitmap->rows; ++Height )
			{
				for (uint32 Width = 0; Width < (uint32)Bitmap->width; ++Width)
				{
					OutRenderData.RawPixels[Height*Bitmap->width+Width] = Bitmap->buffer[Height*Bitmap->pitch+Width] == 1 ? 255 : 0;
				}
			}
		}
	}

	const int32 Height = static_cast<int32>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(FT_MulFix(Face->height, Face->size->metrics.y_scale)) * InScale);

	// Set measurement info for this character
	OutRenderData.GlyphIndex = InFaceGlyphData.GlyphIndex;
	OutRenderData.HasKerning = FT_HAS_KERNING(Face) != 0;
	OutRenderData.MeasureInfo.SizeX = Bitmap->width;
	OutRenderData.MeasureInfo.SizeY = Bitmap->rows;
	OutRenderData.MaxHeight = Height;

	// Ascender is not scaled by freetype.  Scale it now. 
	OutRenderData.MeasureInfo.GlobalAscender = static_cast<int16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(Face->size->metrics.ascender) * InScale);
	// Descender is not scaled by freetype.  Scale it now. 
	OutRenderData.MeasureInfo.GlobalDescender = static_cast<int16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(Face->size->metrics.descender) * InScale);
	// Note we use Slot->advance instead of Slot->metrics.horiAdvance because Slot->Advance contains transformed position (needed if we scale)
	OutRenderData.MeasureInfo.XAdvance = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(Slot->advance.x);
	OutRenderData.MeasureInfo.HorizontalOffset = Slot->bitmap_left;
	OutRenderData.MeasureInfo.VerticalOffset = Slot->bitmap_top;

	if (Slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO)
	{
		FT_Bitmap_Done(FTLibrary->GetLibrary(), Bitmap);
	}

	return true;
}

FT_Face FSlateFontRenderer::GetFontFace(const FFontData& InFontData) const
{
	TSharedPtr<FFreeTypeFace> FaceAndMemory = CompositeFontCache->GetFontFace(InFontData);
	return (FaceAndMemory.IsValid()) ? FaceAndMemory->GetFace() : nullptr;
}

#endif // WITH_FREETYPE
