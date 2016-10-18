// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "FontCacheFreeType.h"

#if WITH_FREETYPE

// The total amount of memory freetype allocates internally
DECLARE_MEMORY_STAT(TEXT("Freetype Total Allocated Memory"), STAT_SlateFreetypeAllocatedMemory, STATGROUP_SlateMemory);

// The total true type memory we are using for active font faces
DECLARE_MEMORY_STAT(TEXT("Uncompressed(in use) TTF/OTF data"), STAT_SlateRawFontDataMemory, STATGROUP_SlateMemory);

namespace FreeTypeMemory
{

static void* Alloc(FT_Memory Memory, long Size)
{
	void* Result = FMemory::Malloc(Size);

#if STATS
	const SIZE_T ActualSize = FMemory::GetAllocSize( Result );
	INC_DWORD_STAT_BY(STAT_SlateFreetypeAllocatedMemory, ActualSize);
#endif

	return Result;
}

static void* Realloc(FT_Memory Memory, long CurSize, long NewSize, void* Block)
{

#if STATS
	long DeltaNewSize = NewSize - CurSize;
	INC_DWORD_STAT_BY(STAT_SlateFreetypeAllocatedMemory, DeltaNewSize);
#endif

	return FMemory::Realloc(Block, NewSize);
}

static void Free(FT_Memory Memory, void* Block)
{	
#if STATS
	const SIZE_T ActualSize = FMemory::GetAllocSize( Block );
	DEC_DWORD_STAT_BY(STAT_SlateFreetypeAllocatedMemory, ActualSize);
#endif
	return FMemory::Free(Block);
}

} // namespace FreeTypeMemory

#endif // WITH_FREETYPE


namespace FreeTypeUtils
{

#if WITH_FREETYPE

void ApplySizeAndScale(FT_Face InFace, const int32 InFontSize, const float InFontScale)
{
	FT_Error Error = FT_Set_Char_Size(InFace, 0, ConvertPixelTo26Dot6<FT_F26Dot6>(InFontSize), FreeTypeConstants::HorizontalDPI, FreeTypeConstants::VerticalDPI);

	check(Error == 0);

	if (InFontScale != 1.0f)
	{
		FT_Matrix ScaleMatrix;
		ScaleMatrix.xy = 0;
		ScaleMatrix.xx = ConvertPixelTo16Dot16<FT_Fixed>(InFontScale);
		ScaleMatrix.yy = ConvertPixelTo16Dot16<FT_Fixed>(InFontScale);
		ScaleMatrix.yx = 0;
		FT_Set_Transform(InFace, &ScaleMatrix, nullptr);
	}
	else
	{
		FT_Set_Transform(InFace, nullptr, nullptr);
	}

	check(Error == 0);
}

FT_Error LoadGlyph(FT_Face InFace, const uint32 InGlyphIndex, const int32 InLoadFlags, const int32 InFontSize, const float InFontScale)
{
	ApplySizeAndScale(InFace, InFontSize, InFontScale);
	return FT_Load_Glyph(InFace, InGlyphIndex, InLoadFlags);
}

#endif // WITH_FREETYPE

}


FFreeTypeLibrary::FFreeTypeLibrary()
{
#if WITH_FREETYPE
	CustomMemory = static_cast<FT_Memory>(FMemory::Malloc(sizeof(*CustomMemory)));

	// Init FreeType
	CustomMemory->alloc = &FreeTypeMemory::Alloc;
	CustomMemory->realloc = &FreeTypeMemory::Realloc;
	CustomMemory->free = &FreeTypeMemory::Free;
	CustomMemory->user = nullptr;

	FT_Error Error = FT_New_Library(CustomMemory, &FTLibrary);
		
	if (Error)
	{
		checkf(0, TEXT("Could not init FreeType. Error code: %d"), Error);
	}
		
	FT_Add_Default_Modules(FTLibrary);
#endif // WITH_FREETYPE
}

FFreeTypeLibrary::~FFreeTypeLibrary()
{
#if WITH_FREETYPE
	FT_Done_Library(FTLibrary);
	FMemory::Free(CustomMemory);
#endif // WITH_FREETYPE
}


FFreeTypeFace::FFreeTypeFace(const FFreeTypeLibrary* InFTLibrary, const void* InRawFontData, const int32 InRawFontDataSizeBytes)
{
#if WITH_FREETYPE
	Memory.Reserve(InRawFontDataSizeBytes);
	Memory.Append(static_cast<const uint8*>(InRawFontData), InRawFontDataSizeBytes);

	FT_New_Memory_Face(InFTLibrary->GetLibrary(), Memory.GetData(), static_cast<FT_Long>(InRawFontDataSizeBytes), 0, &FTFace);

	INC_DWORD_STAT_BY( STAT_SlateRawFontDataMemory, Memory.GetAllocatedSize() );
	if (FTFace)
	{
		// Parse out the font attributes
		TArray<FString> Styles;
		FString(FTFace->style_name).ParseIntoArray(Styles, TEXT(" "), true);

		for (const FString& Style : Styles)
		{
			Attributes.Add(*Style);
		}
	}
#endif // WITH_FREETYPE
}

FFreeTypeFace::~FFreeTypeFace()
{
#if WITH_FREETYPE
	if (FTFace)
	{
		DEC_DWORD_STAT_BY( STAT_SlateRawFontDataMemory, Memory.GetAllocatedSize() );
		FT_Done_Face(FTFace);
		Memory.Empty();
	}
#endif // WITH_FREETYPE
}


#if WITH_FREETYPE

bool FFreeTypeGlyphCache::FindOrCache(FT_Face InFace, const uint32 InGlyphIndex, const int32 InLoadFlags, const int32 InFontSize, const float InFontScale, FCachedGlyphData& OutCachedGlyphData)
{
	const FCachedGlyphKey CachedGlyphKey(InFace, InGlyphIndex, InLoadFlags, InFontSize, InFontScale);

	// Try and find the data from the cache...
	{
		const FCachedGlyphData* FoundCachedGlyphData = CachedGlyphDataMap.Find(CachedGlyphKey);
		if (FoundCachedGlyphData)
		{
			OutCachedGlyphData = *FoundCachedGlyphData;
			return true;
		}
	}

	// No cached data, go ahead and add an entry for it...
	FT_Error Error = FreeTypeUtils::LoadGlyph(InFace, InGlyphIndex, InLoadFlags, InFontSize, InFontScale);
	if (Error == 0)
	{
		OutCachedGlyphData = FCachedGlyphData();
		OutCachedGlyphData.Height = InFace->height;
		OutCachedGlyphData.GlyphMetrics = InFace->glyph->metrics;
		OutCachedGlyphData.SizeMetrics = InFace->size->metrics;

		if (InFace->glyph->outline.n_points > 0)
		{
			const int32 NumPoints = static_cast<int32>(InFace->glyph->outline.n_points);
			OutCachedGlyphData.OutlinePoints.Reserve(NumPoints);
			for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
			{
				OutCachedGlyphData.OutlinePoints.Add(InFace->glyph->outline.points[PointIndex]);
			}
		}

		CachedGlyphDataMap.Add(CachedGlyphKey, OutCachedGlyphData);
		return true;
	}

	return false;
}

#endif // WITH_FREETYPE

void FFreeTypeGlyphCache::FlushCache()
{
#if WITH_FREETYPE
	CachedGlyphDataMap.Empty();
#endif // WITH_FREETYPE
}


#if WITH_FREETYPE

bool FFreeTypeAdvanceCache::FindOrCache(FT_Face InFace, const uint32 InGlyphIndex, const int32 InLoadFlags, const int32 InFontSize, const float InFontScale, FT_Fixed& OutCachedAdvance)
{
	const FCachedAdvanceKey CachedAdvanceKey(InFace, InGlyphIndex, InLoadFlags, InFontSize, InFontScale);

	// Try and find the advance from the cache...
	{
		const FT_Fixed* FoundCachedAdvance = CachedAdvanceMap.Find(CachedAdvanceKey);
		if (FoundCachedAdvance)
		{
			OutCachedAdvance = *FoundCachedAdvance;
			return true;
		}
	}

	FreeTypeUtils::ApplySizeAndScale(InFace, InFontSize, InFontScale);

	// No cached data, go ahead and add an entry for it...
	FT_Error Error = FT_Get_Advance(InFace, InGlyphIndex, InLoadFlags, &OutCachedAdvance);
	if (Error == 0)
	{
		CachedAdvanceMap.Add(CachedAdvanceKey, OutCachedAdvance);
		return true;
	}

	return false;
}

#endif // WITH_FREETYPE

void FFreeTypeAdvanceCache::FlushCache()
{
#if WITH_FREETYPE
	CachedAdvanceMap.Empty();
#endif // WITH_FREETYPE
}


#if WITH_FREETYPE

bool FFreeTypeKerningPairCache::FindOrCache(FT_Face InFace, const FKerningPair& InKerningPair, const int32 InKerningFlags, const int32 InFontSize, const float InFontScale, FT_Vector& OutKerning)
{
	// Skip the cache if the font itself doesn't have kerning
	if (!FT_HAS_KERNING(InFace))
	{
		OutKerning.x = 0;
		OutKerning.y = 0;
		return true;
	}

	const FCachedKerningPairKey CachedKerningPairKey(InFace, InKerningPair, InKerningFlags, InFontSize, InFontScale);

	// Try and find the kerning from the cache...
	{
		const FT_Vector* FoundCachedKerning = CachedKerningPairMap.Find(CachedKerningPairKey);
		if (FoundCachedKerning)
		{
			OutKerning = *FoundCachedKerning;
			return true;
		}
	}

	FreeTypeUtils::ApplySizeAndScale(InFace, InFontSize, 1.0f);

	// No cached data, go ahead and add an entry for it...
	FT_Error Error = FT_Get_Kerning(InFace, InKerningPair.FirstGlyphIndex, InKerningPair.SecondGlyphIndex, InKerningFlags, &OutKerning);
	if (Error == 0)
	{
		if (InKerningFlags != FT_KERNING_UNSCALED)
		{
			// We apply our own scaling as FreeType doesn't always produce the correct results for all fonts when applying the scale via the transform matrix
			const FT_Long FixedFontScale = FreeTypeUtils::ConvertPixelTo16Dot16<FT_Long>(InFontScale);
			OutKerning.x = FT_MulFix(OutKerning.x, FixedFontScale);
			OutKerning.y = FT_MulFix(OutKerning.y, FixedFontScale);
		}

		CachedKerningPairMap.Add(CachedKerningPairKey, OutKerning);
		return true;
	}

	return false;
}

#endif // WITH_FREETYPE

void FFreeTypeKerningPairCache::FlushCache()
{
#if WITH_FREETYPE
	CachedKerningPairMap.Empty();
#endif // WITH_FREETYPE
}
