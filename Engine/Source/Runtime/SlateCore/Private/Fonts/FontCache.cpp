// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "FontCacheFreeType.h"
#include "FontCacheHarfBuzz.h"
#include "FontCacheCompositeFont.h"
#include "SlateFontRenderer.h"
#include "SlateTextShaper.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Font Atlases"), STAT_SlateNumFontAtlases, STATGROUP_SlateMemory);
DECLARE_MEMORY_STAT(TEXT("Font Kerning Table Memory"), STAT_SlateFontKerningTableMemory, STATGROUP_SlateMemory);
DEFINE_STAT(STAT_SlateFontMeasureCacheMemory);


namespace FontCacheConstants
{
	/** Number of characters that can be indexed directly in the cache */
	const int32 DirectAccessSize = 256;
}


static TAutoConsoleVariable<int32> CVarDefaultTextShapingMethod(
	TEXT("Slate.DefaultTextShapingMethod"),
	static_cast<int32>(ETextShapingMethod::Auto),
	TEXT("0: Auto (default), 1: KerningOnly, 2: FullShaping."),
	ECVF_Default
	);

ETextShapingMethod GetDefaultTextShapingMethod()
{
	const int32 DefaultTextShapingMethodAsInt = CVarDefaultTextShapingMethod.AsVariable()->GetInt();
	if (DefaultTextShapingMethodAsInt >= static_cast<int32>(ETextShapingMethod::Auto) && DefaultTextShapingMethodAsInt <= static_cast<int32>(ETextShapingMethod::FullShaping))
	{
		return static_cast<ETextShapingMethod>(DefaultTextShapingMethodAsInt);
	}
	return ETextShapingMethod::Auto;
}


FShapedGlyphEntryKey::FShapedGlyphEntryKey(const TSharedPtr<FShapedGlyphFaceData>& InFontFaceData, uint32 InGlyphIndex)
	: FontFace(InFontFaceData->FontFace)
	, FontSize(InFontFaceData->FontSize)
	, FontScale(InFontFaceData->FontScale)
	, GlyphIndex(InGlyphIndex)
	, KeyHash(0)
{
	KeyHash = HashCombine(KeyHash, GetTypeHash(FontFace));
	KeyHash = HashCombine(KeyHash, GetTypeHash(FontSize));
	KeyHash = HashCombine(KeyHash, GetTypeHash(FontScale));
	KeyHash = HashCombine(KeyHash, GetTypeHash(GlyphIndex));
}


FShapedGlyphSequence::FShapedGlyphSequence(TArray<FShapedGlyphEntry> InGlyphsToRender, TArray<FShapedGlyphClusterBlock> InGlyphClusterBlocks, const int16 InTextBaseline, const uint16 InMaxTextHeight, const UObject* InFontMaterial)
	: GlyphsToRender(MoveTemp(InGlyphsToRender))
	, GlyphClusterBlocks(MoveTemp(InGlyphClusterBlocks))
	, TextBaseline(InTextBaseline)
	, MaxTextHeight(InMaxTextHeight)
	, FontMaterial(InFontMaterial)
{
}

bool FShapedGlyphSequence::IsDirty() const
{
	for (const FShapedGlyphEntry& CurrentGlyph : GlyphsToRender)
	{
		if (!CurrentGlyph.FontFaceData->FontFace.IsValid())
		{
			return true;
		}
	}

	return false;
}

int32 FShapedGlyphSequence::GetMeasuredWidth() const
{
	int32 MeasuredWidth = 0;

	for (const FShapedGlyphEntry& CurrentGlyph : GlyphsToRender)
	{
		MeasuredWidth += CurrentGlyph.XAdvance;
	}

	return MeasuredWidth;
}

TOptional<int32> FShapedGlyphSequence::GetMeasuredWidth(const int32 InStartIndex, const int32 InEndIndex, const bool InIncludeKerningWithPrecedingGlyph) const
{
	int32 MeasuredWidth = 0;

	bool bFoundStartGlyph = false;
	bool bFoundEndGlyph = false;

	const TRange<int32> MeasureRange(InStartIndex, InEndIndex);
	for (const FShapedGlyphClusterBlock& CurrentClusterBlock : GlyphClusterBlocks)
	{
		const TRange<int32> CurrentClusterBlockRange(CurrentClusterBlock.ClusterStartIndex, CurrentClusterBlock.ClusterEndIndex);
		if (CurrentClusterBlockRange.Overlaps(MeasureRange))
		{
			// Measure all the in-range glyphs from this cluster block
			for (int32 CurrentGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex; CurrentGlyphIndex < CurrentClusterBlock.ShapedGlyphEndIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];

				if (!bFoundStartGlyph && CurrentGlyph.ClusterIndex == InStartIndex)
				{
					bFoundStartGlyph = true;
				}

				if (!bFoundEndGlyph && CurrentGlyph.ClusterIndex == InEndIndex)
				{
					bFoundEndGlyph = true;
				}

				if (CurrentGlyph.ClusterIndex >= InStartIndex && CurrentGlyph.ClusterIndex < InEndIndex)
				{
					MeasuredWidth += CurrentGlyph.XAdvance;
				}
			}

			// The shaped glyphs don't contain the end cluster block index, so if we matched end of the cluster range, say we measured okay
			if (CurrentClusterBlock.ClusterEndIndex == InEndIndex)
			{
				bFoundEndGlyph = true;
			}
		}
	}

	if (InIncludeKerningWithPrecedingGlyph && InStartIndex > 0)
	{
		const TOptional<int8> Kerning = GetKerning(InStartIndex - 1);
		MeasuredWidth += Kerning.Get(0);
	}

	// Did we measure okay?
	if (bFoundStartGlyph && bFoundEndGlyph)
	{
		return MeasuredWidth;
	}

	return TOptional<int32>();
}

TOptional<int8> FShapedGlyphSequence::GetKerning(const int32 InIndex) const
{
	for (const FShapedGlyphClusterBlock& CurrentClusterBlock : GlyphClusterBlocks)
	{
		const TRange<int32> CurrentClusterBlockRange(CurrentClusterBlock.ClusterStartIndex, CurrentClusterBlock.ClusterEndIndex);
		if (CurrentClusterBlockRange.Contains(InIndex))
		{
			// Find the correct glyph from this cluster block
			for (int32 CurrentGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex; CurrentGlyphIndex < CurrentClusterBlock.ShapedGlyphEndIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];

				if (CurrentGlyph.ClusterIndex == InIndex)
				{
					return CurrentGlyph.Kerning;
				}
			}

			break;
		}
	}

	// If we got here it means we couldn't find the glyph
	return TOptional<int8>();
}

FShapedGlyphSequencePtr FShapedGlyphSequence::GetSubSequence(const int32 InStartIndex, const int32 InEndIndex) const
{
	TArray<FShapedGlyphEntry> SubGlyphsToRender;
	SubGlyphsToRender.Reserve(InEndIndex - InStartIndex);

	TArray<FShapedGlyphClusterBlock> SubGlyphClusterBlocks;
	SubGlyphClusterBlocks.Reserve(GlyphClusterBlocks.Num());

	bool bFoundStartGlyph = false;
	bool bFoundEndGlyph = false;

	const TRange<int32> MeasureRange(InStartIndex, InEndIndex);
	for (const FShapedGlyphClusterBlock& CurrentClusterBlock : GlyphClusterBlocks)
	{
		const TRange<int32> CurrentClusterBlockRange(CurrentClusterBlock.ClusterStartIndex, CurrentClusterBlock.ClusterEndIndex);
		if (CurrentClusterBlockRange.Overlaps(MeasureRange))
		{
			FShapedGlyphClusterBlock& SubClusterBlock = SubGlyphClusterBlocks[SubGlyphClusterBlocks.AddDefaulted()];
			SubClusterBlock = FShapedGlyphClusterBlock(CurrentClusterBlock.TextDirection, FMath::Max(CurrentClusterBlock.ClusterStartIndex, InStartIndex), FMath::Min(CurrentClusterBlock.ClusterEndIndex, InEndIndex));

			SubClusterBlock.ShapedGlyphStartIndex = SubGlyphsToRender.Num();

			// Add all the in-range glyphs from this cluster block
			for (int32 CurrentGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex; CurrentGlyphIndex < CurrentClusterBlock.ShapedGlyphEndIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];

				if (!bFoundStartGlyph && CurrentGlyph.ClusterIndex == InStartIndex)
				{
					bFoundStartGlyph = true;
				}

				if (!bFoundEndGlyph && CurrentGlyph.ClusterIndex == InEndIndex)
				{
					bFoundEndGlyph = true;
				}

				if (CurrentGlyph.ClusterIndex >= InStartIndex && CurrentGlyph.ClusterIndex < InEndIndex)
				{
					SubGlyphsToRender.Add(CurrentGlyph);
				}
			}

			SubClusterBlock.ShapedGlyphEndIndex = SubGlyphsToRender.Num();

			// The shaped glyphs don't contain the end cluster block index, so if we matched end of the cluster range, say we measured okay
			if (CurrentClusterBlock.ClusterEndIndex == InEndIndex)
			{
				bFoundEndGlyph = true;
			}
		}
	}

	// Did we measure okay?
	if (bFoundStartGlyph && bFoundEndGlyph)
	{
		return MakeShareable(new FShapedGlyphSequence(MoveTemp(SubGlyphsToRender), MoveTemp(SubGlyphClusterBlocks), TextBaseline, MaxTextHeight, FontMaterial));
	}

	return nullptr;
}


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

	if (Character == SlateFontRendererUtils::InvalidSubChar)
	{
		bReturnVal = true;
	}
	else
	{
		float SubFontScalingFactor = 1.0f;
		const FFontData& FontData = FontCache.GetFontDataForCharacter(FontKey.GetFontInfo(), Character, SubFontScalingFactor);

		bReturnVal = FontCache.FontRenderer->CanLoadCharacter(FontData, Character, FontFallback);
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
		else if (Character != SlateFontRendererUtils::InvalidSubChar && FontFallback < ReturnVal->FallbackLevel)
		{
			ReturnVal = NULL;
		}
	}

	// The character is not valid, replace with the invalid character substitute
	if (ReturnVal == NULL)
	{
		ReturnVal = (FCharacterEntry*)&(GetCharacter(SlateFontRendererUtils::InvalidSubChar));
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
	: FTLibrary( new FFreeTypeLibrary() )
	, FTGlyphCache( new FFreeTypeGlyphCache() )
	, FTAdvanceCache( new FFreeTypeAdvanceCache() )
	, FTKerningPairCache( new FFreeTypeKerningPairCache() )
	, CompositeFontCache( new FCompositeFontCache( FTLibrary.Get() ) )
	, FontRenderer( new FSlateFontRenderer( FTLibrary.Get(), FTGlyphCache.Get(), FTKerningPairCache.Get(), CompositeFontCache.Get() ) )
	, TextShaper( new FSlateTextShaper( FTGlyphCache.Get(), FTAdvanceCache.Get(), FTKerningPairCache.Get(), CompositeFontCache.Get(), FontRenderer.Get(), this ) )
	, FontAtlasFactory( InFontAtlasFactory )
	, bFlushRequested( false )
{
	UE_LOG(LogSlate, Log, TEXT("SlateFontCache - WITH_FREETYPE: %d, WITH_HARFBUZZ: %d"), WITH_FREETYPE, WITH_HARFBUZZ);

	FInternationalization::Get().OnCultureChanged().AddRaw(this, &FSlateFontCache::FlushCache);
}

FSlateFontCache::~FSlateFontCache()
{
	FInternationalization::Get().OnCultureChanged().RemoveAll(this);

	// Make sure things get destroyed in the correct order
	TextShaper.Reset();
	FontRenderer.Reset();
	CompositeFontCache.Reset();
	FTKerningPairCache.Reset();
	FTAdvanceCache.Reset();
	FTGlyphCache.Reset();
	FTLibrary.Reset();
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
	const FFontData& FontData = CompositeFontCache->GetFontDataForCharacter( InKey.GetFontInfo(), Character, SubFontScalingFactor );

	// Render the character 
	FCharacterRenderData RenderData;
	EFontFallback CharFallbackLevel;
	const float FontScale = InKey.GetScale() * SubFontScalingFactor;

	FontRenderer->GetRenderData( FontData, InKey.GetFontInfo().Size, Character, RenderData, FontScale, &CharFallbackLevel);

	// the index of the atlas where the character is stored
	int32 AtlasIndex = 0;
	const FAtlasedTextureSlot* NewSlot = nullptr;
	const bool bSuccess = AddNewEntry( RenderData, AtlasIndex, NewSlot );

	if( bSuccess )
	{
		OutCharacterEntry.Character = Character;
		OutCharacterEntry.GlyphIndex = RenderData.GlyphIndex;
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

bool FSlateFontCache::AddNewEntry( const FShapedGlyphEntry& InShapedGlyph, FShapedGlyphFontAtlasData& OutAtlasData ) const
{
	// Render the glyph
	FCharacterRenderData RenderData;
	FontRenderer->GetRenderData(InShapedGlyph, RenderData);

	// The index of the atlas where the character is stored
	int32 AtlasIndex = 0;
	const FAtlasedTextureSlot* NewSlot = nullptr;
	const bool bSuccess = AddNewEntry(RenderData, AtlasIndex, NewSlot);

	if (bSuccess)
	{
		OutAtlasData.VerticalOffset = RenderData.MeasureInfo.VerticalOffset;
		OutAtlasData.HorizontalOffset = RenderData.MeasureInfo.HorizontalOffset;
		OutAtlasData.StartU = NewSlot->X + NewSlot->Padding;
		OutAtlasData.StartV = NewSlot->Y + NewSlot->Padding;
		OutAtlasData.USize = NewSlot->Width - 2 * NewSlot->Padding;
		OutAtlasData.VSize = NewSlot->Height - 2 * NewSlot->Padding;
		OutAtlasData.TextureIndex = AtlasIndex;
		OutAtlasData.Valid = true;
	}

	return bSuccess;
}

bool FSlateFontCache::AddNewEntry( const FCharacterRenderData InRenderData, int32& OutAtlasIndex, const FAtlasedTextureSlot*& OutSlot ) const
{
	for( OutAtlasIndex = 0; OutAtlasIndex < FontAtlases.Num(); ++OutAtlasIndex ) 
	{
		// Add the character to the texture
		OutSlot = FontAtlases[OutAtlasIndex]->AddCharacter( InRenderData );
		if( OutSlot )
		{
			break;
		}
	}

	if( !OutSlot )
	{
		TSharedRef<FSlateFontAtlas> FontAtlas = FontAtlasFactory->CreateFontAtlas();

		// Add the character to the texture
		OutSlot = FontAtlas->AddCharacter( InRenderData );

		OutAtlasIndex = FontAtlases.Add( FontAtlas );

		INC_DWORD_STAT_BY( STAT_SlateNumFontAtlases, 1 );

		if( FontAtlases.Num() > 1 )
		{
			// There is more than one font atlas which means there is a lot of font data being cached
			// try to shrink it next time
			bFlushRequested = true;
		}
	}

	return OutSlot != nullptr;
}

FShapedGlyphSequenceRef FSlateFontCache::ShapeBidirectionalText( const FString& InText, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InBaseDirection, const ETextShapingMethod InTextShapingMethod ) const
{
	return ShapeBidirectionalText(*InText, 0, InText.Len(), InFontInfo, InFontScale, InBaseDirection, InTextShapingMethod);
}

FShapedGlyphSequenceRef FSlateFontCache::ShapeBidirectionalText( const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InBaseDirection, const ETextShapingMethod InTextShapingMethod ) const
{
	return TextShaper->ShapeBidirectionalText(InText, InTextStart, InTextLen, InFontInfo, InFontScale, InBaseDirection, InTextShapingMethod);
}

FShapedGlyphSequenceRef FSlateFontCache::ShapeUnidirectionalText( const FString& InText, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod InTextShapingMethod ) const
{
	return ShapeUnidirectionalText(*InText, 0, InText.Len(), InFontInfo, InFontScale, InTextDirection, InTextShapingMethod);
}

FShapedGlyphSequenceRef FSlateFontCache::ShapeUnidirectionalText( const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod InTextShapingMethod ) const
{
	return TextShaper->ShapeUnidirectionalText(InText, InTextStart, InTextLen, InFontInfo, InFontScale, InTextDirection, InTextShapingMethod);
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
			FlushData();
		}
		else
		{
			return CachedCharacterList->Get();
		}
	}

	return FontToCharacterListCache.Add( FontKey, MakeShareable( new FCharacterList( FontKey, *this ) ) ).Get();
}

FShapedGlyphFontAtlasData FSlateFontCache::GetShapedGlyphFontAtlasData( const FShapedGlyphEntry& InShapedGlyph ) const
{
	check(IsInGameThread() || IsInRenderingThread());

	const ESlateTextureAtlasThreadId AtlasThreadId = GetCurrentSlateTextureAtlasThreadId();
	check(AtlasThreadId != ESlateTextureAtlasThreadId::Unknown);

	const int32 CachedAtlasDataIndex = (AtlasThreadId == ESlateTextureAtlasThreadId::Game) ? 0 : 1;

	// Has the atlas data already been cached on the glyph?
	{
		TSharedPtr<FShapedGlyphFontAtlasData> CachedAtlasDataPin = InShapedGlyph.CachedAtlasData[CachedAtlasDataIndex].Pin();
		if (CachedAtlasDataPin.IsValid())
		{
			return *CachedAtlasDataPin;
		}
	}

	// Not cached on the glyph, so create a key for to look up this glyph, as it may
	// have already been cached by another shaped text sequence
	const FShapedGlyphEntryKey GlyphKey(InShapedGlyph.FontFaceData, InShapedGlyph.GlyphIndex);

	// Has the atlas data already been cached by another shaped text sequence?
	const TSharedRef<FShapedGlyphFontAtlasData>* FoundAtlasData = ShapedGlyphToAtlasData.Find(GlyphKey);
	if (FoundAtlasData)
	{
		InShapedGlyph.CachedAtlasData[CachedAtlasDataIndex] = *FoundAtlasData;
		return **FoundAtlasData;
	}

	// Not cached at all... create a new entry
	TSharedRef<FShapedGlyphFontAtlasData> NewAtlasData = MakeShareable(new FShapedGlyphFontAtlasData());
	AddNewEntry(InShapedGlyph, *NewAtlasData);

	if (NewAtlasData->Valid)
	{
		InShapedGlyph.CachedAtlasData[CachedAtlasDataIndex] = NewAtlasData;
		ShapedGlyphToAtlasData.Add(GlyphKey, NewAtlasData);
	}

	return *NewAtlasData;
}

const FFontData& FSlateFontCache::GetDefaultFontData( const FSlateFontInfo& InFontInfo ) const
{
	return CompositeFontCache->GetDefaultFontData(InFontInfo);
}

const FFontData& FSlateFontCache::GetFontDataForCharacter( const FSlateFontInfo& InFontInfo, const TCHAR InChar, float& OutScalingFactor ) const
{
	return CompositeFontCache->GetFontDataForCharacter(InFontInfo, InChar, OutScalingFactor);
}

uint16 FSlateFontCache::GetMaxCharacterHeight( const FSlateFontInfo& InFontInfo, float FontScale ) const
{
	return FontRenderer->GetMaxHeight(InFontInfo, FontScale);
}

int16 FSlateFontCache::GetBaseline( const FSlateFontInfo& InFontInfo, float FontScale ) const
{
	return FontRenderer->GetBaseline(InFontInfo, FontScale);
}

int8 FSlateFontCache::GetKerning( const FFontData& InFontData, const int32 InSize, TCHAR First, TCHAR Second, float Scale ) const
{
	return FontRenderer->GetKerning(InFontData, InSize, First, Second, Scale);
}

bool FSlateFontCache::HasKerning( const FFontData& InFontData ) const
{
	return FontRenderer->HasKerning(InFontData);
}

const TSet<FName>& FSlateFontCache::GetFontAttributes( const FFontData& InFontData ) const
{
	return CompositeFontCache->GetFontAttributes(InFontData);
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
		FlushData();
	}
}

bool FSlateFontCache::ConditionalFlushCache()
{
	bool bFlushed = false;
	if( bFlushRequested )
	{
		bFlushRequested = false;
		FlushCache();
		bFlushed = !bFlushRequested;
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
	if ( DoesThreadOwnSlateRendering() )
	{
		FlushData();

		FontToCharacterListCache.Empty();
		ShapedGlyphToAtlasData.Empty();

		for ( int32 AtlasIndex = 0; AtlasIndex < FontAtlases.Num(); ++AtlasIndex )
		{
			FontAtlases[AtlasIndex]->ReleaseResources();
		}

		// hack
		FSlateApplicationBase::Get().GetRenderer()->FlushCommands();

		SET_DWORD_STAT(STAT_SlateNumFontAtlases, 0);

		FontAtlases.Empty();

		UE_LOG(LogSlate, Verbose, TEXT("Slate font cache was flushed"));
	}
	else
	{
		bFlushRequested = true;
	}
}

void FSlateFontCache::FlushData() const
{
	// Ensure all invalidation panels are cleared of cached widgets
	FSlateApplicationBase::Get().InvalidateAllWidgets();

	FTGlyphCache->FlushCache();
	FTAdvanceCache->FlushCache();
	FTKerningPairCache->FlushCache();
	CompositeFontCache->FlushCache();
}
