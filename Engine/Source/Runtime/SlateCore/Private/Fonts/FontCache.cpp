// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "LegacySlateFontInfoCache.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Font Atlases"), STAT_SlateNumFontAtlases, STATGROUP_SlateMemory);
DECLARE_MEMORY_STAT(TEXT("Font Kerning Table Memory"), STAT_SlateFontKerningTableMemory, STATGROUP_SlateMemory);
DEFINE_STAT(STAT_SlateFontMeasureCacheMemory);

#ifndef WITH_FREETYPE
	#define WITH_FREETYPE	0
#endif // WITH_FREETYPE

#if PLATFORM_COMPILER_HAS_GENERIC_KEYWORD
	#define generic __identifier(generic)
#endif	//PLATFORM_COMPILER_HAS_GENERIC_KEYWORD

#if WITH_FREETYPE
	#include "ft2build.h"

	// Freetype style include
	#include FT_FREETYPE_H
	#include FT_GLYPH_H
	#include FT_MODULE_H
	#include FT_BITMAP_H

#endif // WITH_FREETYPE

namespace FontCacheConstants
{
	/** The horizontal dpi we render at */
	const uint32 HorizontalDPI = 96;
	/** The vertical dpi we render at */
	const uint32 VerticalDPI = 96;

	/** Number of characters that can be indexed directly in the cache */
	const int32 DirectAccessSize = 256;

	/** Number of possible elements in each measurement cache */
	const uint32 MeasureCacheSize = 500;
}

// Character used to substitute invalid font characters
const TCHAR InvalidSubChar = '\u001A';

#if WITH_FREETYPE
const uint32 GlobalGlyphFlags = FT_LOAD_NO_BITMAP;

/**
 * Memory allocation functions to be used only by freetype
 */
static void* FreetypeAlloc( FT_Memory Memory, long size )
{
	return FMemory::Malloc( size );
}

static void* FreetypeRealloc( FT_Memory Memory, long CurSize, long NewSize, void* Block )
{
	return FMemory::Realloc( Block, NewSize );
}

static void FreetypeFree( FT_Memory Memory, void* Block )
{
	return FMemory::Free( Block );
}

#endif // WITH_FREETYPE


/**
 * Helper for pop/pushing the font fallback level, within function calls
 */
class FScopedFontFallback
{
public:
	FScopedFontFallback(EFontFallback& OutFontFallback, EFontFallback SetFontFallback)
		: FontFallback(OutFontFallback)
		, OldFontFallback(OutFontFallback)
	{
		FontFallback = SetFontFallback;
	}

	~FScopedFontFallback()
	{
		FontFallback = OldFontFallback;
	}

private:
	EFontFallback& FontFallback;
	EFontFallback OldFontFallback;
};

/**
 * Cached data for a given typeface
 */
class FCachedTypefaceData
{
public:
	/** Default constructor */
	FCachedTypefaceData()
		: Typeface(nullptr)
		, SingularFontData(nullptr)
		, NameToFontDataMap()
		, ScalingFactor(1.0f)
	{
	}

	/** Construct the cache from the given typeface */
	FCachedTypefaceData(const FTypeface& InTypeface, const float InScalingFactor = 1.0f)
		: Typeface(&InTypeface)
		, SingularFontData(nullptr)
		, NameToFontDataMap()
		, ScalingFactor(InScalingFactor)
	{
		if(InTypeface.Fonts.Num() == 0)
		{
			// We have no entries - don't bother building a map
			SingularFontData = nullptr;
		}
		else if(InTypeface.Fonts.Num() == 1)
		{
			// We have a single entry - don't bother building a map
			SingularFontData = &InTypeface.Fonts[0].Font;
		}
		else
		{
			// Add all the entries from the typeface
			for(const FTypefaceEntry& TypefaceEntry : InTypeface.Fonts)
			{
				NameToFontDataMap.Add(TypefaceEntry.Name, &TypefaceEntry.Font);
			}

			// Add a special "None" entry to return the first font from the typeface
			if(!NameToFontDataMap.Contains(NAME_None))
			{
				NameToFontDataMap.Add(NAME_None, &InTypeface.Fonts[0].Font);
			}
		}
	}

	/** Get the typeface we cached data from */
	const FTypeface& GetTypeface() const
	{
		check(Typeface);
		return *Typeface;
	}

	/** Find the font associated with the given name */
	const FFontData* GetFontData(const FName& InName) const
	{
		if(NameToFontDataMap.Num() > 0)
		{
			const FFontData* const * const FoundFontData = NameToFontDataMap.Find(InName);
			return (FoundFontData) ? *FoundFontData : nullptr;
		}
		return SingularFontData;
	}

	/** Get the scaling factor for this typeface */
	float GetScalingFactor() const
	{
		return ScalingFactor;
	}

private:
	/** Typeface we cached data from */
	const FTypeface* Typeface;

	/** Singular entry, used when we don't have enough data to warrant using a map */
	const FFontData* SingularFontData;

	/** Mapping between a font name, and its data */
	TMap<FName, const FFontData*> NameToFontDataMap;

	/** Scaling factor to apply to this typeface */
	float ScalingFactor;
};

/**
 * Cached data for a given composite font
 */
class FCachedCompositeFontData
{
public:
	/** Default constructor */
	FCachedCompositeFontData()
		: CompositeFont(nullptr)
		, CachedTypefaces()
		, CachedFontRanges()
	{
	}

	/** Construct the cache from the given composite font */
	FCachedCompositeFontData(const FCompositeFont& InCompositeFont)
		: CompositeFont(&InCompositeFont)
		, CachedTypefaces()
		, CachedFontRanges()
	{
		// Add all the entries from the composite font
		CachedTypefaces.Add(MakeShareable(new FCachedTypefaceData(InCompositeFont.DefaultTypeface)));
		for(const FCompositeSubFont& SubTypeface : InCompositeFont.SubTypefaces)
		{
			TSharedPtr<FCachedTypefaceData> CachedTypeface = MakeShareable(new FCachedTypefaceData(SubTypeface.Typeface, SubTypeface.ScalingFactor));
			CachedTypefaces.Add(CachedTypeface);

			for(const FInt32Range& Range : SubTypeface.CharacterRanges)
			{
				CachedFontRanges.Add(FCachedFontRange(Range, CachedTypeface));
			}
		}

		// Sort the font ranges into ascending order
		CachedFontRanges.Sort([](const FCachedFontRange& RangeOne, const FCachedFontRange& RangeTwo) -> bool
		{
			if(RangeOne.Range.IsEmpty() && !RangeTwo.Range.IsEmpty())
			{
				return true;
			}
			if(!RangeOne.Range.IsEmpty() && RangeTwo.Range.IsEmpty())
			{
				return false;
			}
			return RangeOne.Range.GetLowerBoundValue() < RangeTwo.Range.GetLowerBoundValue();
		});
	}

	/** Get the composite font we cached data from */
	const FCompositeFont& GetCompositeFont() const
	{
		check(CompositeFont);
		return *CompositeFont;
	}

	/** Get the default typeface for this composite font */
	const FCachedTypefaceData* GetDefaultTypeface() const
	{
		return CachedTypefaces[0].Get();
	}

	/** Get the typeface that should be used for the given character */
	const FCachedTypefaceData* GetTypefaceForCharacter(const TCHAR InChar) const
	{
		const int32 CharIndex = static_cast<int32>(InChar);

		for(const FCachedFontRange& CachedRange : CachedFontRanges)
		{
			if(CachedRange.Range.IsEmpty())
			{
				continue;
			}

			// Ranges are sorting in ascending order (by the start position), so if this range starts higher than the character we're looking for, we can bail from the check
			if(CachedRange.Range.GetLowerBoundValue() > CharIndex)
			{
				break;
			}

			if(CachedRange.Range.Contains(CharIndex))
			{
				return CachedRange.CachedTypeface.Get();
			}
		}

		return CachedTypefaces[0].Get();
	}

private:
	/** Entry containing a range and the typeface associated with that range */
	struct FCachedFontRange
	{
		/** Default constructor */
		FCachedFontRange()
			: Range(FInt32Range::Empty())
			, CachedTypeface()
		{
		}

		/** Construct from the given range and typeface */
		FCachedFontRange(const FInt32Range& InRange, TSharedPtr<FCachedTypefaceData> InCachedTypeface)
			: Range(InRange)
			, CachedTypeface(MoveTemp(InCachedTypeface))
		{
		}

		/** Range to use for the typeface */
		FInt32Range Range;

		/** Typeface to which the range applies */
		TSharedPtr<FCachedTypefaceData> CachedTypeface;
	};

	/** Composite font we cached data from */
	const FCompositeFont* CompositeFont;

	/** Array of cached typefaces - 0 is the default typeface, and the remaining entries are sub-typefaces */
	TArray<TSharedPtr<FCachedTypefaceData>> CachedTypefaces;

	/** Array of font ranges paired with their associated typefaces - this is sorted in ascending order */
	TArray<FCachedFontRange> CachedFontRanges;
};

/**
 * An interface to the freetype API.                     
 */
class FFreeTypeInterface
{
#if WITH_FREETYPE
	/**
	 * Internal struct for passing around information about loading a character
	 */
	struct FFaceCharData
	{
		/** The font face for the character */
		FT_Face Face;

		/** The glyph index for the character */
		FT_UInt GlyphIndex;

		/** The glyph flags that should be used for loading the characters glyph */
		uint32 GlyphFlags;

		/** The fallback font set the character was loaded from */
		EFontFallback CharFallbackLevel;


		FFaceCharData()
			: Face(NULL)
			, GlyphIndex(0)
			, GlyphFlags(0)
			, CharFallbackLevel(EFontFallback::FF_NoFallback)
		{
		}
	};
#endif

public:
	FFreeTypeInterface()
	{
#if WITH_FREETYPE
		CustomMemory = (FT_Memory)FMemory::Malloc( sizeof(*CustomMemory) );
		// Init freetype
		CustomMemory->alloc = FreetypeAlloc;
		CustomMemory->realloc = FreetypeRealloc;
		CustomMemory->free = FreetypeFree;
		CustomMemory->user = nullptr;

		int32 Error = FT_New_Library( CustomMemory, &FTLibrary );
		
		if ( Error )
		{
			checkf(0, TEXT("Could not init Freetype"));
		}
		
		FT_Add_Default_Modules( FTLibrary );
#endif // WITH_FREETYPE
	}

	~FFreeTypeInterface()
	{
#if WITH_FREETYPE
		// Clear before releasing freetype resources
		Flush();

		FT_Done_Library( FTLibrary );
		FMemory::Free( CustomMemory );
#endif // WITH_FREETYPE
	}

	/**
	 * Flushes stored data.
	 */ 
	void Flush()
	{
#if WITH_FREETYPE
		FontToKerningPairMap.Empty();
		// toss memory
		for (auto& FontFaceEntry : FontFaceMap)
		{
			FT_Done_Face(FontFaceEntry.Value.Face);
			FMemory::Free(FontFaceEntry.Value.Memory);
		}
		FontFaceMap.Empty();
		CompositeFontToCachedDataMap.Empty();
#endif // WITH_FREETYPE
	}

	/**
	 * @return The global max height for any character in the default font
	 */
	uint16 GetMaxHeight( const FSlateFontInfo& InFontInfo, const float InScale )
	{
#if WITH_FREETYPE
		const FFontData& FontData = GetDefaultFontData(InFontInfo);

		// Just render the null character 
		TCHAR Char = 0;

		// Render the character 
		FCharacterRenderData NewRenderData;
		GetRenderData(FontData, InFontInfo.Size, Char, NewRenderData, InScale);

		return NewRenderData.MaxHeight;
#else
		return 0;
#endif // WITH_FREETYPE
	}

	/** 
	 * @return the baseline for any character in the default font
	 */
	int16 GetBaseline( const FSlateFontInfo& InFontInfo, const float InScale )
	{
#if WITH_FREETYPE
		// Just get info for the null character 
		TCHAR Char = 0;
		const FFontData& FontData = GetDefaultFontData(InFontInfo);
		FFaceCharData CharData = GetFontFaceForCharacter(FontData, Char, EFontFallback::FF_Max);

		check(CharData.Face != NULL);

		FT_Error Error = LoadGlyph(FontData, CharData, InFontInfo.Size, InScale);


		check(Error == 0);

		return (CharData.Face->size->metrics.descender / 64) * InScale;
#else
		return 0;
#endif // WITH_FREETYPE
	}


	/** 
	 * Creates render data for a specific character 
	 * 
	 * @param InFontData		Raw font data to render the character with
	 * @param InSize			The size of the font to draw
	 * @param Char				The character to render
	 * @param OutRenderData		Will contain the created render data
	 * @param InScale			The scale of the font
	 * @param OutFallbackLevel	Outputs the fallback level of the font
	 */
	void GetRenderData(const FFontData& InFontData, const int32 InSize, TCHAR Char, FCharacterRenderData& OutRenderData,
						const float InScale, EFontFallback* OutFallbackLevel=NULL)
	{
#if WITH_FREETYPE
		FFaceCharData CharData = GetFontFaceForCharacter(InFontData, Char, EFontFallback::FF_Max);

		check(CharData.Face != NULL);

		if (OutFallbackLevel != NULL)
		{
			*OutFallbackLevel = CharData.CharFallbackLevel;
		}

		FT_Error Error = LoadGlyph(InFontData, CharData, InSize, InScale);

		check(Error==0);


		// Get the slot for the glyph.  This contains measurement info
		FT_GlyphSlot Slot = CharData.Face->glyph;
		
		FT_Render_Glyph( Slot, FT_RENDER_MODE_NORMAL );

		// one byte per pixel 
		const uint32 GlyphPixelSize = 1;

		FT_Bitmap* Bitmap = nullptr;

		FT_Bitmap NewBitmap;
		if( Slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO )
		{
			FT_Bitmap_New( &NewBitmap );
			// Convert the mono font to 8bbp from 1bpp
			FT_Bitmap_Convert( FTLibrary, &Slot->bitmap, &NewBitmap, 4 );

			Bitmap = &NewBitmap;
		}
		else
		{
			Bitmap = &Slot->bitmap;
		}
		
		OutRenderData.RawPixels.Reset();
		OutRenderData.RawPixels.AddUninitialized( Bitmap->rows * Bitmap->width );

		// Nothing to do for zero width or height glyphs
		if (OutRenderData.RawPixels.Num())
		{
			// Copy the rendered bitmap to our raw pixels array

			if( Slot->bitmap.pixel_mode != FT_PIXEL_MODE_MONO )
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
				for(uint32 Height = 0; Height < (uint32)Bitmap->rows; ++Height )
				{
					for( uint32 Width = 0; Width < (uint32)Bitmap->width; ++Width )
					{
						OutRenderData.RawPixels[Height*Bitmap->width+Width] = Bitmap->buffer[Height*Bitmap->pitch+Width] == 1 ? 255 : 0;
					}
				}
			}
		}

		FT_BBox GlyphBox;
		FT_Glyph Glyph;
		FT_Get_Glyph( Slot, &Glyph );
		FT_Glyph_Get_CBox( Glyph, FT_GLYPH_BBOX_PIXELS, &GlyphBox );

		int32 Height = (FT_MulFix( CharData.Face->height, CharData.Face->size->metrics.y_scale ) / 64) * InScale;

		// Set measurement info for this character
		OutRenderData.Char = Char;
		OutRenderData.HasKerning = FT_HAS_KERNING( CharData.Face ) != 0;
		OutRenderData.MeasureInfo.SizeX = Bitmap->width;
		OutRenderData.MeasureInfo.SizeY = Bitmap->rows;
		OutRenderData.MaxHeight = Height;

		// Need to divide by 64 to get pixels;
		// Ascender is not scaled by freetype.  Scale it now. 
		OutRenderData.MeasureInfo.GlobalAscender = ( CharData.Face->size->metrics.ascender / 64 ) * InScale;
		// Descender is not scaled by freetype.  Scale it now. 
		OutRenderData.MeasureInfo.GlobalDescender = ( CharData.Face->size->metrics.descender / 64 ) * InScale;
		// Note we use Slot->advance instead of Slot->metrics.horiAdvance because Slot->Advance contains transformed position (needed if we scale)
		OutRenderData.MeasureInfo.XAdvance =  Slot->advance.x / 64;
		OutRenderData.MeasureInfo.HorizontalOffset = Slot->bitmap_left;
		OutRenderData.MeasureInfo.VerticalOffset = Slot->bitmap_top;

		if( Slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO )
		{
			FT_Bitmap_Done( FTLibrary, Bitmap );
		}

		FT_Done_Glyph( Glyph );

#endif // WITH_FREETYPE
	}

	/** Get the default font data to use for the given font info */
	const FFontData& GetDefaultFontData(const FSlateFontInfo& InFontInfo)
	{
		static const FFontData DummyFontData;

#if WITH_FREETYPE
		const FCompositeFont* const ResolvedCompositeFont = InFontInfo.GetCompositeFont();
		const FCachedTypefaceData* const CachedTypefaceData = GetDefaultCachedTypeface(ResolvedCompositeFont);
		if(CachedTypefaceData)
		{
			// Try to find the correct font from the typeface
			const FFontData* FoundFontData = CachedTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
			if(FoundFontData)
			{
				return *FoundFontData;
			}

			// Failing that, return the first font available (the "None" font)
			FoundFontData = CachedTypefaceData->GetFontData(NAME_None);
			if(FoundFontData)
			{
				return *FoundFontData;
			}
		}
#endif // WITH_FREETYPE

		return DummyFontData;
	}

	/** Get the font data to use for the given font info and character */
	const FFontData& GetFontDataForCharacter(const FSlateFontInfo& InFontInfo, const TCHAR InChar, float& OutScalingFactor)
	{
		static const FFontData DummyFontData;

#if WITH_FREETYPE
		const FCompositeFont* const ResolvedCompositeFont = InFontInfo.GetCompositeFont();
		const FCachedTypefaceData* const CachedTypefaceData = GetCachedTypefaceForCharacter(ResolvedCompositeFont, InChar);
		if(CachedTypefaceData)
		{
			OutScalingFactor = CachedTypefaceData->GetScalingFactor();

			// Try to find the correct font from the typeface
			const FFontData* FoundFontData = CachedTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
			if(FoundFontData)
			{
				return *FoundFontData;
			}

			// Failing that, try and find a font by the attributes of the default font with the given name
			const FCachedTypefaceData* const CachedDefaultTypefaceData = GetDefaultCachedTypeface(ResolvedCompositeFont);
			if(CachedDefaultTypefaceData && CachedTypefaceData != CachedDefaultTypefaceData)
			{
				const FFontData* const FoundDefaultFontData = CachedDefaultTypefaceData->GetFontData(InFontInfo.TypefaceFontName);
				if(FoundDefaultFontData)
				{
					const TSet<FName>& DefaultFontAttributes = GetFontAttributes(*FoundDefaultFontData);
					FoundFontData = GetBestMatchFontForAttributes(CachedTypefaceData, DefaultFontAttributes);
					if(FoundFontData)
					{
						return *FoundFontData;
					}
				}
			}

			// Failing that, return the first font available (the "None" font)
			FoundFontData = CachedTypefaceData->GetFontData(NAME_None);
			if(FoundFontData)
			{
				return *FoundFontData;
			}
		}
#endif // WITH_FREETYPE

		OutScalingFactor = 1.0f;
		return DummyFontData;
	}

	/**
	 * @param Whether or not the font has kerning
	 */
	bool HasKerning( const FFontData& InFontData )
	{
#if WITH_FREETYPE
		FT_Face FontFace = GetFontFace( InFontData );

		if ( FontFace == nullptr )
		{
			return false;
		}

		return FT_HAS_KERNING( FontFace ) != 0;
#else
		return false;
#endif // WITH_FREETYPE
	}

	/**
	 * Calculates the kerning amount for a pair of characters
	 *
	 * @param InFontData	The font that used to draw the string with the first and second characters
	 * @param InSize		The size of the font to draw
	 * @param First			The first character in the pair
	 * @param Second		The second character in the pair
	 * @return The kerning amount, 0 if no kerning
	 */
	int8 GetKerning( const FFontData& InFontData, const int32 InSize, TCHAR First, TCHAR Second, const float InScale )
	{
#if WITH_FREETYPE
		int32 Kerning = 0;
		int32* FoundKerning = nullptr;

		FT_Face FontFace = GetFontFace( InFontData );

		// Check if this font has kerning as not all fonts do.
		// We also can't perform kerning between two separate font faces
		if( FontFace != nullptr && FT_HAS_KERNING( FontFace ) )
		{
			FT_Error Error = FT_Set_Char_Size( FontFace, 0, InSize*64, FontCacheConstants::HorizontalDPI, FontCacheConstants::VerticalDPI  );

			if( InScale != 1.0f )
			{
				FT_Matrix ScaleMatrix;
				ScaleMatrix.xy = 0;
				ScaleMatrix.xx = (FT_Fixed)(InScale * 65536);
				ScaleMatrix.yy = (FT_Fixed)(InScale * 65536);
				ScaleMatrix.yx = 0;
				FT_Set_Transform( FontFace, &ScaleMatrix, nullptr );
			}
			else
			{
				FT_Set_Transform( FontFace, nullptr, nullptr );
			}

			check(Error==0);
		
			int32 KernValue = 0;
		
			FT_UInt FirstIndex = FT_Get_Char_Index( FontFace, First );
			FT_UInt SecondIndex = FT_Get_Char_Index( FontFace, Second );

			FT_Vector KerningVec;
			FT_Get_Kerning( FontFace, FirstIndex, SecondIndex, FT_KERNING_DEFAULT, &KerningVec );

			// Return pixel sizes
			Kerning = KerningVec.x / 64;
		}

		return Kerning;
#else
		return 0;
#endif // WITH_FREETYPE
	}

	/** Get the attributes associated with the given font data */
	const TSet<FName>& GetFontAttributes( const FFontData& InFontData )
	{
		static const TSet<FName> DummyAttributes;

#if WITH_FREETYPE
		FFontFaceAndMemory* FaceAndMemory = FontFaceMap.Find(&InFontData);
		if (!FaceAndMemory)
		{
			GetFontFace(InFontData); // will try and create the entry
			FaceAndMemory = FontFaceMap.Find(&InFontData);
		}

		return (FaceAndMemory) ? FaceAndMemory->Attributes : DummyAttributes;
#else
		return DummyAttributes;
#endif // WITH_FREETYPE
	}


	/**
	 * Whether or not the specified character, within the specified font, can be loaded with the specified maximum font fallback level
	 *
	 * @param InFontData		Information about the font to load
	 * @param Char				The character being loaded
	 * @param MaxFallbackLevel	The maximum fallback level to try for the font
	 * @return					Whether or not the character can be loaded
	 */
	bool CanLoadCharacter(const FFontData& InFontData, TCHAR Char, EFontFallback MaxFallbackLevel)
	{
		bool bReturnVal = false;

#if WITH_FREETYPE
		FFaceCharData CharData = GetFontFaceForCharacter(InFontData, Char, MaxFallbackLevel);

		bReturnVal = CharData.Face != NULL && CharData.GlyphIndex != 0;
#endif

		return bReturnVal;
	}

private:

#if WITH_FREETYPE
	/** Get the cached composite font data for the given composite font */
	const FCachedCompositeFontData* GetCachedCompositeFont(const FCompositeFont* const InCompositeFont)
	{
		if(!InCompositeFont)
		{
			return nullptr;
		}

		TSharedPtr<FCachedCompositeFontData>* const FoundCompositeFontData = CompositeFontToCachedDataMap.Find(InCompositeFont);
		if(FoundCompositeFontData)
		{
			return FoundCompositeFontData->Get();
		}

		return CompositeFontToCachedDataMap.Add(InCompositeFont, MakeShareable(new FCachedCompositeFontData(*InCompositeFont))).Get();
	}

	/** Get the default typeface for the given composite font */
	const FCachedTypefaceData* GetDefaultCachedTypeface(const FCompositeFont* const InCompositeFont)
	{
		const FCachedCompositeFontData* const CachedCompositeFont = GetCachedCompositeFont(InCompositeFont);
		return (CachedCompositeFont) ? CachedCompositeFont->GetDefaultTypeface() : nullptr;
	}

	/** Get the typeface that should be used for the given character */
	const FCachedTypefaceData* GetCachedTypefaceForCharacter(const FCompositeFont* const InCompositeFont, const TCHAR InChar)
	{
		const FCachedCompositeFontData* const CachedCompositeFont = GetCachedCompositeFont(InCompositeFont);
		return (CachedCompositeFont) ? CachedCompositeFont->GetTypefaceForCharacter(InChar) : nullptr;
	}

	const FFontData* GetBestMatchFontForAttributes(const FCachedTypefaceData* const InCachedTypefaceData, const TSet<FName>& InFontAttributes)
	{
		const FFontData* BestMatchFont = nullptr;
		int32 BestMatchCount = 0;

		const FTypeface& Typeface = InCachedTypefaceData->GetTypeface();
		for(const FTypefaceEntry& TypefaceEntry : Typeface.Fonts)
		{
			const TSet<FName>& FontAttributes = GetFontAttributes(TypefaceEntry.Font);

			int32 MatchCount = 0;
			for(const FName& InAttribute : InFontAttributes)
			{
				if(FontAttributes.Contains(InAttribute))
				{
					++MatchCount;
				}
			}

			if(MatchCount > BestMatchCount || !BestMatchFont)
			{
				BestMatchFont = &TypefaceEntry.Font;
				BestMatchCount = MatchCount;
			}
		}

		return BestMatchFont;
	}


	/**
	 * Gets or loads a freetype font face
	 *
	 * @param InFontData Information about the font to load
	 */
	FT_Face GetFontFace( const FFontData& InFontData )
	{
		FFontFaceAndMemory* FaceAndMemory = FontFaceMap.Find(&InFontData);
		if (!FaceAndMemory && InFontData.BulkDataPtr)
		{
			int32 LockedFontDataSizeBytes = 0;
			const void* const LockedFontData = InFontData.BulkDataPtr->Lock(LockedFontDataSizeBytes);
			if (LockedFontDataSizeBytes > 0)
			{
				// make a new entry
				FaceAndMemory = &FontFaceMap.Add(&InFontData, FFontFaceAndMemory());

				FaceAndMemory->Memory = static_cast<uint8*>(FMemory::Malloc(LockedFontDataSizeBytes));
				FMemory::Memcpy(FaceAndMemory->Memory, LockedFontData, LockedFontDataSizeBytes);

				// initialize the font, setting the error code
				const bool bFailedToLoadFace = FT_New_Memory_Face(FTLibrary, FaceAndMemory->Memory, static_cast<FT_Long>(LockedFontDataSizeBytes), 0, &FaceAndMemory->Face) != 0;

				// if it failed, we don't want to keep the memory around
				if (bFailedToLoadFace)
				{
					FMemory::Free(FaceAndMemory->Memory);
					FontFaceMap.Remove(&InFontData);
					FaceAndMemory = nullptr;
					UE_LOG(LogSlate, Warning, TEXT("GetFontFace failed to load or process '%s'"), *InFontData.FontFilename);
				}
			
				if (FaceAndMemory)
				{
					// Parse out the font attributes
					TArray<FString> Styles;
					FString(FaceAndMemory->Face->style_name).ParseIntoArray(Styles, TEXT(" "), true);

					for (const FString& Style : Styles)
					{
						FaceAndMemory->Attributes.Add(*Style);
					}
				}
			}

			InFontData.BulkDataPtr->Unlock();
		}

		return (FaceAndMemory) ? FaceAndMemory->Face : nullptr;
	}


	/**
	 * Wrapper for above, which reverts to fallback or last resort fonts if the face could not be loaded
	 *
	 * @param InFontData		Information about the font to load
	 * @param Char				The character being loaded (required for checking if a fallback font is needed)
	 * @param MaxFallbackLevel	The maximum fallback level to try for the font
	 * @return					Returns the character font face data
	 */
	FFaceCharData GetFontFaceForCharacter(const FFontData& InFontData, TCHAR Char, EFontFallback MaxFallbackLevel)
	{
		FFaceCharData ReturnVal;

		FT_Face Face = GetFontFace(InFontData);
		FT_UInt GlyphIndex = 0;
		bool bOverrideFallback = Char == InvalidSubChar;

		if (Face != NULL) 
		{
			// Get the index to the glyph in the font face
			GlyphIndex = FT_Get_Char_Index(Face, Char);
			ReturnVal.CharFallbackLevel = EFontFallback::FF_NoFallback;
		}

		// If the requested glyph doesn't exist, use the localization fallback font
		if (Face == NULL || (Char != 0 && GlyphIndex == 0))
		{
			bool bCanFallback = bOverrideFallback || MaxFallbackLevel >= EFontFallback::FF_LocalizedFallback;

			if (bCanFallback)
			{
				Face = GetFontFace(FLegacySlateFontInfoCache::Get().GetFallbackFontData());

				if (Face != NULL)
				{	
					GlyphIndex = FT_Get_Char_Index(Face, Char);

					ReturnVal.CharFallbackLevel = EFontFallback::FF_LocalizedFallback;
					ReturnVal.GlyphFlags |= FT_LOAD_FORCE_AUTOHINT;
				}
			}
		}

		// If the requested glyph doesn't exist, use the last resort fallback font
		if (Face == NULL || (Char != 0 && GlyphIndex == 0))
		{
			bool bCanFallback = bOverrideFallback || MaxFallbackLevel >= EFontFallback::FF_LastResortFallback;

			if (bCanFallback)
			{
				Face = GetFontFace(FLegacySlateFontInfoCache::Get().GetLastResortFontData());

				if (Face != NULL)
				{
					GlyphIndex = FT_Get_Char_Index(Face, Char);

					ReturnVal.CharFallbackLevel = EFontFallback::FF_LastResortFallback;
					ReturnVal.GlyphFlags |= FT_LOAD_FORCE_AUTOHINT;
				}
			}
		}

		ReturnVal.Face = Face;
		ReturnVal.GlyphIndex = GlyphIndex;

		return ReturnVal;
	}


	/**
	 * Helper split from GetRenderData, for use with GetBaseline
	 */
	FT_Error LoadGlyph(const FFontData& InFontData, FFaceCharData& InCharData, const int32 InSize, const float InScale)
	{
		// Set the character size to render at (needs to be in 1/64 of a "point")
		FT_Error Error = FT_Set_Char_Size(InCharData.Face, 0, InSize*64, FontCacheConstants::HorizontalDPI,
											FontCacheConstants::VerticalDPI);

		check(Error==0);

		if( InScale != 1.0f )
		{
			FT_Matrix ScaleMatrix;
			ScaleMatrix.xy = 0;
			ScaleMatrix.xx = (FT_Fixed)(InScale * 65536);
			ScaleMatrix.yy = (FT_Fixed)(InScale * 65536);
			ScaleMatrix.yx = 0;
			FT_Set_Transform( InCharData.Face, &ScaleMatrix, nullptr );
		}
		else
		{
			FT_Set_Transform( InCharData.Face, nullptr, nullptr );
		}


		// Setup additional glyph flags
		InCharData.GlyphFlags |= GlobalGlyphFlags;

		switch(InFontData.Hinting)
		{
		case EFontHinting::Auto:		InCharData.GlyphFlags |= FT_LOAD_FORCE_AUTOHINT; break;
		case EFontHinting::AutoLight:	InCharData.GlyphFlags |= FT_LOAD_TARGET_LIGHT; break;
		case EFontHinting::Monochrome:	InCharData.GlyphFlags |= FT_LOAD_TARGET_MONO | FT_LOAD_FORCE_AUTOHINT; break;
		case EFontHinting::None:		InCharData.GlyphFlags |= FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_HINTING; break;
		case EFontHinting::Default:
		default:						InCharData.GlyphFlags |= FT_LOAD_TARGET_NORMAL; break;
		}

		// Load the glyph.  Force using the freetype hinter because not all true type fonts have their own hinting
		Error = FT_Load_Glyph( InCharData.Face, InCharData.GlyphIndex, InCharData.GlyphFlags );

		return Error;
	}


private:
	struct FFontFaceAndMemory
	{
		// the FT2 object
		FT_Face Face;

		// the memory for the face (can't be a TArray as the FontFaceMap could be reallocated and copy it's members around)
		uint8* Memory;

		// The attributes (read from the FT_Face, but split into a more usable structure)
		TSet<FName> Attributes;

		FFontFaceAndMemory()
			: Face(nullptr)
			, Memory(nullptr)
			, Attributes()
		{
		}
	};

	/** Mapping of font data to freetype faces */
	TMap<const FFontData*,FFontFaceAndMemory> FontFaceMap;
	/** Mapping of fonts to maps of kerning pairs */
	TMap<FSlateFontInfo, TMap<FKerningPair,int32> > FontToKerningPairMap;
	/** Mapping of composite fonts to their cached lookup data */
	TMap<const FCompositeFont*, TSharedPtr<FCachedCompositeFontData>> CompositeFontToCachedDataMap;
	/** Free type library interface */
	FT_Library FTLibrary;
	FT_Memory CustomMemory;
#endif // WITH_FREETYPE
};


FKerningTable::FKerningTable( const FSlateFontCache& InFontCache )
	: DirectAccessTable( nullptr )
	, FontCache( InFontCache )
{
}

FKerningTable::~FKerningTable()
{
	if( DirectAccessTable )
	{
		const uint32 DirectAccessSizeBytes = FontCacheConstants::DirectAccessSize*FontCacheConstants::DirectAccessSize*sizeof(int8);

		FMemory::Free( DirectAccessTable );

		DEC_MEMORY_STAT_BY( STAT_SlateFontKerningTableMemory, DirectAccessSizeBytes );
	}
	DEC_MEMORY_STAT_BY( STAT_SlateFontKerningTableMemory, MappedKerningPairs.GetAllocatedSize() );
}

int8 FKerningTable::GetKerning( const FFontData& InFontData, const int32 InSize, TCHAR FirstChar, TCHAR SecondChar, float InScale )
{
	int8 OutKerning = 0;

	if( FirstChar < FontCacheConstants::DirectAccessSize && SecondChar < FontCacheConstants::DirectAccessSize )
	{
		// This character can be directly indexed

		if( !DirectAccessTable )
		{
			// Create the table now
			CreateDirectTable();
		}

		// Determine the index into the kerning table
		const uint32 Index = FirstChar*FontCacheConstants::DirectAccessSize+SecondChar;

		OutKerning = DirectAccessTable[Index];
		// If the kerning value hasn't been accessed yet, get the value from the cache now 
		if( OutKerning == MAX_int8 )
		{
			OutKerning = FontCache.GetKerning( InFontData, InSize, FirstChar, SecondChar, InScale );
			DirectAccessTable[Index] = OutKerning;
		}
	}
	else
	{
		// Kerning is mapped
		FKerningPair KerningPair( FirstChar, SecondChar );

		int8* FoundKerning = MappedKerningPairs.Find( KerningPair );
		if( !FoundKerning )
		{
			OutKerning = FontCache.GetKerning( InFontData, InSize, FirstChar, SecondChar, InScale );

			STAT(const uint32 CurrentMemoryUsage = MappedKerningPairs.GetAllocatedSize());
			MappedKerningPairs.Add( KerningPair, OutKerning );
			STAT(
			{
				uint32 NewMemoryUsage = MappedKerningPairs.GetAllocatedSize();
				if (NewMemoryUsage > CurrentMemoryUsage)
				{
					INC_MEMORY_STAT_BY(STAT_SlateFontKerningTableMemory, NewMemoryUsage - CurrentMemoryUsage);
				}
			})
		}
	}

	return OutKerning;
}

void FKerningTable::CreateDirectTable()
{
	const uint32 DirectAccessSizeBytes = FontCacheConstants::DirectAccessSize*FontCacheConstants::DirectAccessSize*sizeof(int8);

	check( !DirectAccessTable )
	DirectAccessTable = (int8*)FMemory::Malloc(DirectAccessSizeBytes);
	// Invalidate all entries so we know when to get a new kerning value from the implementation
	FMemory::Memset( DirectAccessTable, MAX_int8, DirectAccessSizeBytes );
	INC_MEMORY_STAT_BY( STAT_SlateFontKerningTableMemory, DirectAccessSizeBytes );
}

FCharacterList::FCharacterList( const FSlateFontKey& InFontKey, const FSlateFontCache& InFontCache )
	: KerningTable( InFontCache )
	, FontKey( InFontKey )
	, FontCache( InFontCache )
	, CompositeFontHistoryRevision( 0 )
	, MaxDirectIndexedEntries( FontCacheConstants::DirectAccessSize )
	, MaxHeight( 0 )
	, Baseline( 0 )
	, FontFallback(EFontFallback::FF_Max)
{
	const FCompositeFont* const CompositeFont = InFontKey.GetFontInfo().GetCompositeFont();
	if( CompositeFont )
	{
		CompositeFontHistoryRevision = CompositeFont->HistoryRevision;
	}
}

bool FCharacterList::IsStale() const
{
	const FCompositeFont* const CompositeFont = FontKey.GetFontInfo().GetCompositeFont();
	return !CompositeFont || CompositeFontHistoryRevision != CompositeFont->HistoryRevision;
}

int8 FCharacterList::GetKerning(const FSlateFontInfo& InFontInfo, TCHAR FirstChar, TCHAR SecondChar)
{
	FScopedFontFallback FallbackScope(FontFallback, InFontInfo.FontFallback);

	const FCharacterEntry First = GetCharacter( FirstChar );
	const FCharacterEntry Second = GetCharacter( SecondChar );
	return GetKerning( First, Second );
}

int8 FCharacterList::GetKerning( const FCharacterEntry& FirstCharacterEntry, const FCharacterEntry& SecondCharacterEntry )
{
	// We can only get kerning if both characters are using the same font
	if( FirstCharacterEntry.FontData && 
		FirstCharacterEntry.FontData->BulkDataPtr && 
		FirstCharacterEntry.HasKerning && 
		*FirstCharacterEntry.FontData == *SecondCharacterEntry.FontData )
	{
		return KerningTable.GetKerning( 
			*FirstCharacterEntry.FontData, 
			FontKey.GetFontInfo().Size, 
			FirstCharacterEntry.Character, 
			SecondCharacterEntry.Character, 
			FirstCharacterEntry.FontScale 
			);
	}

	return 0;
}

uint16 FCharacterList::GetMaxHeight() const
{
	if( MaxHeight == 0 )
	{
		MaxHeight = FontCache.GetMaxCharacterHeight( FontKey.GetFontInfo(), FontKey.GetScale() );
	}

	return MaxHeight;
}

int16 FCharacterList::GetBaseline() const
{
	if (Baseline == 0)
	{
		Baseline = FontCache.GetBaseline( FontKey.GetFontInfo(), FontKey.GetScale() );
	}

	return Baseline;
}

bool FCharacterList::CanCacheCharacter(TCHAR Character)
{
	bool bReturnVal = false;

	if (Character == InvalidSubChar)
	{
		bReturnVal = true;
	}
	else
	{
		float SubFontScalingFactor = 1.0f;
		const FFontData& FontData = FontCache.GetFontDataForCharacter(FontKey.GetFontInfo(), Character, SubFontScalingFactor);

		bReturnVal = FontCache.FTInterface->CanLoadCharacter(FontData, Character, FontFallback);
	}

	return bReturnVal;
}

const FCharacterEntry& FCharacterList::GetCharacter(const FSlateFontInfo& InFontInfo, TCHAR Character)
{
	FScopedFontFallback FallbackScope(FontFallback, InFontInfo.FontFallback);

	return GetCharacter(Character);
}

const FCharacterEntry& FCharacterList::GetCharacter(TCHAR Character)
{
	FCharacterEntry* ReturnVal = NULL;
	bool bDirectIndexChar = Character < MaxDirectIndexedEntries;

	// First get a reference to the character, if it is already mapped (mapped does not mean cached though)
	if (bDirectIndexChar)
	{
		if (DirectIndexEntries.IsValidIndex(Character))
		{
			ReturnVal = &DirectIndexEntries[Character];
		}
	}
	else
	{
		ReturnVal = MappedEntries.Find(Character);
	}


	// Determine whether the character needs caching, and map it if needed
	bool bNeedCaching = false;

	if (ReturnVal != NULL)
	{
		bNeedCaching = !ReturnVal->IsCached();

		// If the character needs caching, but can't be cached, reject the character
		if (bNeedCaching && !CanCacheCharacter(Character))
		{
			bNeedCaching = false;
			ReturnVal = NULL;
		}
	}
	// Only map the character if it can be cached
	else if (CanCacheCharacter(Character))
	{
		bNeedCaching = true;

		if (bDirectIndexChar)
		{
			DirectIndexEntries.AddZeroed((Character - DirectIndexEntries.Num()) + 1);
			ReturnVal = &DirectIndexEntries[Character];
		}
		else
		{
			ReturnVal = &(MappedEntries.Add(Character));
		}
	}


	if (ReturnVal != NULL)
	{
		if (bNeedCaching)
		{
			ReturnVal = &(CacheCharacter(Character));
		}
		// For already-cached characters, reject characters that don't fall within maximum font fallback level requirements
		else if (Character != InvalidSubChar && FontFallback < ReturnVal->FallbackLevel)
		{
			ReturnVal = NULL;
		}
	}

	// The character is not valid, replace with the invalid character substitute
	if (ReturnVal == NULL)
	{
		ReturnVal = (FCharacterEntry*)&(GetCharacter(InvalidSubChar));
	}

	return *ReturnVal;
}

FCharacterEntry& FCharacterList::CacheCharacter( TCHAR Character )
{
	FCharacterEntry NewEntry;
	bool bSuccess = FontCache.AddNewEntry( Character, FontKey, NewEntry );

	check( bSuccess );

	if( Character < MaxDirectIndexedEntries && NewEntry.IsCached() )
	{
		DirectIndexEntries[ Character ] = NewEntry;
		return DirectIndexEntries[ Character ];
	}
	else
	{
		return MappedEntries.Add( Character, NewEntry );
	}
}

FSlateFontCache::FSlateFontCache( TSharedRef<ISlateFontAtlasFactory> InFontAtlasFactory )
	: FTInterface( new FFreeTypeInterface )
	, FontAtlasFactory( InFontAtlasFactory )
	, bFlushRequested( false )
{

}

FSlateFontCache::~FSlateFontCache()
{	

}

int32 FSlateFontCache::GetNumAtlasPages() const
{
	return FontAtlases.Num();
}

FIntPoint FSlateFontCache::GetAtlasPageSize() const
{
	return FontAtlasFactory->GetAtlasSize();
}

FSlateShaderResource* FSlateFontCache::GetAtlasPageResource(const int32 InIndex) const
{
	return FontAtlases[InIndex]->GetSlateTexture();
}

bool FSlateFontCache::IsAtlasPageResourceAlphaOnly() const
{
	return true;
}

bool FSlateFontCache::AddNewEntry( TCHAR Character, const FSlateFontKey& InKey, FCharacterEntry& OutCharacterEntry ) const
{
	float SubFontScalingFactor = 1.0f;
	const FFontData& FontData = FTInterface->GetFontDataForCharacter( InKey.GetFontInfo(), Character, SubFontScalingFactor );

	// Render the character 
	FCharacterRenderData RenderData;
	EFontFallback CharFallbackLevel;
	const float FontScale = InKey.GetScale() * SubFontScalingFactor;

	FTInterface->GetRenderData( FontData, InKey.GetFontInfo().Size, Character, RenderData, FontScale, &CharFallbackLevel);

	// the index of the atlas where the character is stored
	int32 AtlasIndex = 0;
	const FAtlasedTextureSlot* NewSlot = nullptr;

	for( AtlasIndex = 0; AtlasIndex < FontAtlases.Num(); ++AtlasIndex ) 
	{
		// Add the character to the texture
		NewSlot = FontAtlases[AtlasIndex]->AddCharacter( RenderData );
		if( NewSlot )
		{
			break;
		}
	}

	if( !NewSlot )
	{
		TSharedRef<FSlateFontAtlas> FontAtlas = FontAtlasFactory->CreateFontAtlas();

		// Add the character to the texture
		NewSlot = FontAtlas->AddCharacter( RenderData );

		AtlasIndex = FontAtlases.Add( FontAtlas );

		INC_DWORD_STAT_BY( STAT_SlateNumFontAtlases, 1 );

		if( FontAtlases.Num() > 1 )
		{
			// There is more than one font atlas which means there is a lot of font data being cached
			// try to shrink it next time
			bFlushRequested = true;
		}
	}

	// If the new slot is null and we are not just measuring, then the atlas is full
	bool bSuccess = NewSlot != nullptr;

	if( bSuccess )
	{
		OutCharacterEntry.Character = Character;
		OutCharacterEntry.FontData = &FontData;
		OutCharacterEntry.FontScale = FontScale;
		OutCharacterEntry.StartU = NewSlot->X + NewSlot->Padding;
		OutCharacterEntry.StartV = NewSlot->Y + NewSlot->Padding;
		OutCharacterEntry.USize = NewSlot->Width - 2 * NewSlot->Padding;
		OutCharacterEntry.VSize = NewSlot->Height - 2 * NewSlot->Padding;
		OutCharacterEntry.TextureIndex = 0;
		OutCharacterEntry.XAdvance = RenderData.MeasureInfo.XAdvance;
		OutCharacterEntry.VerticalOffset = RenderData.MeasureInfo.VerticalOffset;
		OutCharacterEntry.GlobalDescender = GetBaseline(InKey.GetFontInfo(), InKey.GetScale()); // All fonts within a composite font need to use the baseline of the default font
		OutCharacterEntry.HorizontalOffset = RenderData.MeasureInfo.HorizontalOffset;
		OutCharacterEntry.TextureIndex = AtlasIndex;
		OutCharacterEntry.HasKerning = RenderData.HasKerning;
		OutCharacterEntry.FallbackLevel = CharFallbackLevel;
		OutCharacterEntry.Valid = 1;
	}

	return bSuccess;
}

FCharacterList& FSlateFontCache::GetCharacterList( const FSlateFontInfo &InFontInfo, float FontScale ) const
{
	// Create a key for looking up each character
	const FSlateFontKey FontKey( InFontInfo, FontScale );

	TSharedRef< class FCharacterList >* CachedCharacterList = FontToCharacterListCache.Find( FontKey );

	if( CachedCharacterList )
	{
		// Clear out this entry if it's stale so that we make a new one
		if( (*CachedCharacterList)->IsStale() )
		{
			FontToCharacterListCache.Remove( FontKey );
			FTInterface->Flush();
		}
		else
		{
			return CachedCharacterList->Get();
		}
	}

	return FontToCharacterListCache.Add( FontKey, MakeShareable( new FCharacterList( FontKey, *this ) ) ).Get();
}

const FFontData& FSlateFontCache::GetDefaultFontData( const FSlateFontInfo& InFontInfo ) const
{
	return FTInterface->GetDefaultFontData(InFontInfo);
}

const FFontData& FSlateFontCache::GetFontDataForCharacter( const FSlateFontInfo& InFontInfo, const TCHAR InChar, float& OutScalingFactor ) const
{
	return FTInterface->GetFontDataForCharacter(InFontInfo, InChar, OutScalingFactor);
}

uint16 FSlateFontCache::GetMaxCharacterHeight( const FSlateFontInfo& InFontInfo, float FontScale ) const
{
	return FTInterface->GetMaxHeight(InFontInfo, FontScale);
}

int16 FSlateFontCache::GetBaseline( const FSlateFontInfo& InFontInfo, float FontScale ) const
{
	return FTInterface->GetBaseline(InFontInfo, FontScale);
}

int8 FSlateFontCache::GetKerning( const FFontData& InFontData, const int32 InSize, TCHAR First, TCHAR Second, float Scale ) const
{
	return FTInterface->GetKerning(InFontData, InSize, First, Second, Scale);
}

bool FSlateFontCache::HasKerning( const FFontData& InFontData ) const
{
	return FTInterface->HasKerning(InFontData);
}

const TSet<FName>& FSlateFontCache::GetFontAttributes( const FFontData& InFontData ) const
{
	return FTInterface->GetFontAttributes(InFontData);
}

void FSlateFontCache::FlushObject( const UObject* const InObject )
{
	if( !InObject )
	{
		return;
	}

	bool bHasRemovedEntries = false;
	for( auto It = FontToCharacterListCache.CreateIterator(); It; ++It )
	{
		if( It.Key().GetFontInfo().FontObject == InObject)
		{
			bHasRemovedEntries = true;
			It.RemoveCurrent();
		}
	}

	if( bHasRemovedEntries )
	{
		FTInterface->Flush();
	}
}

bool FSlateFontCache::ConditionalFlushCache()
{
	bool bFlushed = false;
	if( bFlushRequested )
	{
		FlushCache();
		bFlushed = true;
		bFlushRequested = false;
	}

	return bFlushed;
}

void FSlateFontCache::UpdateCache()
{
	for( int32 AtlasIndex = 0; AtlasIndex < FontAtlases.Num(); ++AtlasIndex ) 
	{
		FontAtlases[AtlasIndex]->ConditionalUpdateTexture();
	}
}

void FSlateFontCache::ReleaseResources()
{
	for( int32 AtlasIndex = 0; AtlasIndex < FontAtlases.Num(); ++AtlasIndex ) 
	{
		FontAtlases[AtlasIndex]->ReleaseResources();
	}
}

void FSlateFontCache::FlushCache() const
{
	// Ensure all invalidation panels are cleared of cached widgets
	FSlateApplicationBase::Get().InvalidateAllWidgets();

	FontToCharacterListCache.Empty();
	FTInterface->Flush();

	for( int32 AtlasIndex = 0; AtlasIndex < FontAtlases.Num(); ++AtlasIndex ) 
	{
		FontAtlases[AtlasIndex]->ReleaseResources();
	}

	// hack
	FSlateApplicationBase::Get().GetRenderer()->FlushCommands();

	SET_DWORD_STAT( STAT_SlateNumFontAtlases, 0 );

	FontAtlases.Empty();

	UE_LOG( LogSlate, Verbose, TEXT("Slate font cache was flushed") );
}
