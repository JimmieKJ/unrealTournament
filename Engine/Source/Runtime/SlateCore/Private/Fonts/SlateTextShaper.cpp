// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "SlateTextShaper.h"
#include "SlateFontRenderer.h"
#include "FontCacheFreeType.h"
#include "FontCacheCompositeFont.h"
#include "FontCache.h"


namespace
{

struct FKerningOnlyTextSequenceEntry
{
	int32 TextStartIndex;
	int32 TextLength;
	const FFontData* FontDataPtr;
	TSharedPtr<FFreeTypeFace> FaceAndMemory;
	float SubFontScalingFactor;

	FKerningOnlyTextSequenceEntry(const int32 InTextStartIndex, const int32 InTextLength, const FFontData* InFontDataPtr, TSharedPtr<FFreeTypeFace> InFaceAndMemory, const float InSubFontScalingFactor)
		: TextStartIndex(InTextStartIndex)
		, TextLength(InTextLength)
		, FontDataPtr(InFontDataPtr)
		, FaceAndMemory(MoveTemp(InFaceAndMemory))
		, SubFontScalingFactor(InSubFontScalingFactor)
	{
	}
};

#if WITH_HARFBUZZ

struct FHarfBuzzTextSequenceEntry
{
	struct FSubSequenceEntry
	{
		int32 StartIndex;
		int32 Length;
		hb_script_t HarfBuzzScript;

		FSubSequenceEntry(const int32 InStartIndex, const int32 InLength, const hb_script_t InHarfBuzzScript)
			: StartIndex(InStartIndex)
			, Length(InLength)
			, HarfBuzzScript(InHarfBuzzScript)
		{
		}
	};

	int32 TextStartIndex;
	int32 TextLength;
	const FFontData* FontDataPtr;
	TSharedPtr<FFreeTypeFace> FaceAndMemory;
	float SubFontScalingFactor;
	TArray<FSubSequenceEntry> SubSequence;

	FHarfBuzzTextSequenceEntry(const int32 InTextStartIndex, const int32 InTextLength, const FFontData* InFontDataPtr, TSharedPtr<FFreeTypeFace> InFaceAndMemory, const float InSubFontScalingFactor)
		: TextStartIndex(InTextStartIndex)
		, TextLength(InTextLength)
		, FontDataPtr(InFontDataPtr)
		, FaceAndMemory(MoveTemp(InFaceAndMemory))
		, SubFontScalingFactor(InSubFontScalingFactor)
	{
	}
};

#endif // WITH_HARFBUZZ

} // anonymous namespace


FSlateTextShaper::FSlateTextShaper(FFreeTypeGlyphCache* InFTGlyphCache, FFreeTypeAdvanceCache* InFTAdvanceCache, FFreeTypeKerningPairCache* InFTKerningPairCache, FCompositeFontCache* InCompositeFontCache, FSlateFontRenderer* InFontRenderer, FSlateFontCache* InFontCache)
	: FTGlyphCache(InFTGlyphCache)
	, FTAdvanceCache(InFTAdvanceCache)
	, FTKerningPairCache(InFTKerningPairCache)
	, CompositeFontCache(InCompositeFontCache)
	, FontRenderer(InFontRenderer)
	, FontCache(InFontCache)
	, TextBiDiDetection(TextBiDi::CreateTextBiDi())
#if WITH_HARFBUZZ
	, HarfBuzzFontFactory(FTGlyphCache, FTAdvanceCache, FTKerningPairCache)
#endif // WITH_HARFBUZZ
{
	check(FTGlyphCache);
	check(FTAdvanceCache);
	check(FTKerningPairCache);
	check(CompositeFontCache);
	check(FontRenderer);
	check(FontCache);
}

FShapedGlyphSequenceRef FSlateTextShaper::ShapeBidirectionalText(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InBaseDirection, const ETextShapingMethod TextShapingMethod) const
{
	TArray<TextBiDi::FTextDirectionInfo> TextDirectionInfos;
	TextBiDiDetection->ComputeTextDirection(InText, InTextStart, InTextLen, InBaseDirection, TextDirectionInfos);

	TArray<FShapedGlyphEntry> GlyphsToRender;
	TArray<FShapedGlyphClusterBlock> GlyphClusterBlocks;

	for (const TextBiDi::FTextDirectionInfo& TextDirectionInfo : TextDirectionInfos)
	{
		FShapedGlyphClusterBlock& GlyphClusterBlock = GlyphClusterBlocks[GlyphClusterBlocks.AddDefaulted()];
		PerformTextShaping(InText, TextDirectionInfo.StartIndex, TextDirectionInfo.Length, InFontInfo, InFontScale, TextDirectionInfo.TextDirection, TextShapingMethod, GlyphsToRender, GlyphClusterBlock);
	}

	return FinalizeTextShaping(MoveTemp(GlyphsToRender), MoveTemp(GlyphClusterBlocks), InFontInfo, InFontScale);
}

FShapedGlyphSequenceRef FSlateTextShaper::ShapeUnidirectionalText(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod TextShapingMethod) const
{
	TArray<FShapedGlyphEntry> GlyphsToRender;
	TArray<FShapedGlyphClusterBlock> GlyphClusterBlocks;

	{
		FShapedGlyphClusterBlock& GlyphClusterBlock = GlyphClusterBlocks[GlyphClusterBlocks.AddDefaulted()];
		PerformTextShaping(InText, InTextStart, InTextLen, InFontInfo, InFontScale, InTextDirection, TextShapingMethod, GlyphsToRender, GlyphClusterBlock);
	}

	return FinalizeTextShaping(MoveTemp(GlyphsToRender), MoveTemp(GlyphClusterBlocks), InFontInfo, InFontScale);
}

void FSlateTextShaper::PerformTextShaping(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, const ETextShapingMethod TextShapingMethod, TArray<FShapedGlyphEntry>& OutGlyphsToRender, FShapedGlyphClusterBlock& OutGlyphClusterBlock) const
{
	check(InTextDirection != TextBiDi::ETextDirection::Mixed);

	OutGlyphClusterBlock = FShapedGlyphClusterBlock(InTextDirection, InTextStart, InTextStart + InTextLen);
	OutGlyphClusterBlock.ShapedGlyphStartIndex = OutGlyphsToRender.Num();

#if WITH_FREETYPE
	if (InTextLen > 0)
	{
#if WITH_HARFBUZZ
		if (TextShapingMethod == ETextShapingMethod::FullShaping || (TextShapingMethod == ETextShapingMethod::Auto && InTextDirection == TextBiDi::ETextDirection::RightToLeft))
		{
			PerformHarfBuzzTextShaping(InText, InTextStart, InTextLen, InFontInfo, InFontScale, InTextDirection, OutGlyphsToRender);
		}
		else
#endif // WITH_HARFBUZZ
		{
			PerformKerningOnlyTextShaping(InText, InTextStart, InTextLen, InFontInfo, InFontScale, OutGlyphsToRender);
		}
	}
#endif // WITH_FREETYPE

	OutGlyphClusterBlock.ShapedGlyphEndIndex = OutGlyphsToRender.Num();
}

FShapedGlyphSequenceRef FSlateTextShaper::FinalizeTextShaping(TArray<FShapedGlyphEntry> InGlyphsToRender, TArray<FShapedGlyphClusterBlock> InGlyphClusterBlocks, const FSlateFontInfo &InFontInfo, const float InFontScale) const
{
	int16 TextBaseline = 0;
	uint16 MaxHeight = 0;

#if WITH_FREETYPE
	{
		// Just get info for the null character
		TCHAR Char = 0;
		const FFontData& FontData = CompositeFontCache->GetDefaultFontData(InFontInfo);
		const FFreeTypeFaceGlyphData FaceGlyphData = FontRenderer->GetFontFaceForCharacter(FontData, Char, EFontFallback::FF_Max);

		if (FaceGlyphData.FaceAndMemory.IsValid())
		{
			FFreeTypeGlyphCache::FCachedGlyphData CachedGlyphData;
			if (FTGlyphCache->FindOrCache(FaceGlyphData.FaceAndMemory->GetFace(), FaceGlyphData.GlyphIndex, FaceGlyphData.GlyphFlags, InFontInfo.Size, InFontScale, CachedGlyphData))
			{
				TextBaseline = static_cast<int16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(CachedGlyphData.SizeMetrics.descender) * InFontScale);
				MaxHeight = static_cast<uint16>(FreeTypeUtils::Convert26Dot6ToRoundedPixel<int32>(FT_MulFix(CachedGlyphData.Height, CachedGlyphData.SizeMetrics.y_scale)) * InFontScale);
			}
		}
	}
#endif // WITH_FREETYPE

	return MakeShareable(new FShapedGlyphSequence(MoveTemp(InGlyphsToRender), MoveTemp(InGlyphClusterBlocks), TextBaseline, MaxHeight, InFontInfo.FontMaterial));
}

#if WITH_FREETYPE

void FSlateTextShaper::PerformKerningOnlyTextShaping(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, TArray<FShapedGlyphEntry>& OutGlyphsToRender) const
{
	// We need to work out the correct FFontData for everything so that we can build accurate FShapedGlyphFaceData for rendering later on
	TArray<FKerningOnlyTextSequenceEntry> KerningOnlyTextSequence;

	// Step 1) Split the text into sections that are using the same font face (composite fonts may contain different faces for different character ranges)
	{
		// Data used while detecting font face boundaries
		int32 SplitStartIndex = InTextStart;
		int32 RunningTextIndex = InTextStart;
		const FFontData* RunningFontDataPtr = nullptr;
		TSharedPtr<FFreeTypeFace> RunningFaceAndMemory;
		float RunningSubFontScalingFactor = 1.0f;

		auto AppendPendingFontDataToSequence = [&]()
		{
			if (RunningFontDataPtr)
			{
				KerningOnlyTextSequence.Emplace(
					SplitStartIndex,						// InTextStartIndex
					RunningTextIndex - SplitStartIndex,		// InTextLength
					RunningFontDataPtr,						// InFontDataPtr
					RunningFaceAndMemory,					// InFaceAndMemory
					RunningSubFontScalingFactor				// InSubFontScalingFactor
					);

				RunningFontDataPtr = nullptr;
				RunningFaceAndMemory.Reset();
				RunningSubFontScalingFactor = 1.0f;
			}
		};

		const int32 TextEndIndex = InTextStart + InTextLen;
		for (; RunningTextIndex < TextEndIndex; ++RunningTextIndex)
		{
			float SubFontScalingFactor = 1.0f;
			const FFontData& FontData = CompositeFontCache->GetFontDataForCharacter(InFontInfo, InText[RunningTextIndex], SubFontScalingFactor);
			const FFreeTypeFaceGlyphData FaceGlyphData = FontRenderer->GetFontFaceForCharacter(FontData, InText[RunningTextIndex], EFontFallback::FF_Max);

			if (!RunningFontDataPtr || RunningFontDataPtr != &FontData || RunningFaceAndMemory != FaceGlyphData.FaceAndMemory || RunningSubFontScalingFactor != SubFontScalingFactor)
			{
				AppendPendingFontDataToSequence();

				SplitStartIndex = RunningTextIndex;
				RunningFontDataPtr = &FontData;
				RunningFaceAndMemory = FaceGlyphData.FaceAndMemory;
				RunningSubFontScalingFactor = SubFontScalingFactor;
			}
		}

		AppendPendingFontDataToSequence();
	}

	// Step 2) Now we use the font cache to get the size for each character, and kerning for each character pair
	{
		FCharacterList& CharacterList = FontCache->GetCharacterList(InFontInfo, InFontScale);
		OutGlyphsToRender.Reserve(OutGlyphsToRender.Num() + InTextLen);
		for (const FKerningOnlyTextSequenceEntry& KerningOnlyTextSequenceEntry : KerningOnlyTextSequence)
		{
			const float FinalFontScale = InFontScale * KerningOnlyTextSequenceEntry.SubFontScalingFactor;

			uint32 GlyphFlags = 0;
			SlateFontRendererUtils::AppendGlyphFlags(*KerningOnlyTextSequenceEntry.FontDataPtr, GlyphFlags);

			TSharedRef<FShapedGlyphFaceData> ShapedGlyphFaceData = MakeShareable(new FShapedGlyphFaceData(KerningOnlyTextSequenceEntry.FaceAndMemory, GlyphFlags, InFontInfo.Size, FinalFontScale));

			FCharacterEntry PreviousEntry;
			for (int32 SequenceCharIndex = 0; SequenceCharIndex < KerningOnlyTextSequenceEntry.TextLength; ++SequenceCharIndex)
			{
				const int32 CurrentCharIndex = KerningOnlyTextSequenceEntry.TextStartIndex + SequenceCharIndex;
				const TCHAR CurrentChar = InText[CurrentCharIndex];

				const FCharacterEntry& Entry = CharacterList.GetCharacter(InFontInfo, CurrentChar);

				int8 Kerning = 0;
				if (PreviousEntry.Valid)
				{
					Kerning = CharacterList.GetKerning(PreviousEntry, Entry);
				}

				PreviousEntry = Entry;

				const int32 CurrentGlyphEntryIndex = OutGlyphsToRender.AddDefaulted();
				FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex];
				ShapedGlyphEntry.FontFaceData = ShapedGlyphFaceData;
				ShapedGlyphEntry.GlyphIndex = Entry.GlyphIndex;
				ShapedGlyphEntry.ClusterIndex = CurrentCharIndex;
				ShapedGlyphEntry.XAdvance = Entry.XAdvance;
				ShapedGlyphEntry.YAdvance = 0;
				ShapedGlyphEntry.XOffset = 0;
				ShapedGlyphEntry.YOffset = 0;
				ShapedGlyphEntry.Kerning = 0;

				// Apply the kerning against the previous entry now that we know what it is
				if (CurrentGlyphEntryIndex > 0 && Kerning != 0)
				{
					FShapedGlyphEntry& PreviousShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex - 1];
					PreviousShapedGlyphEntry.XAdvance += Kerning;
					PreviousShapedGlyphEntry.Kerning = Kerning;
				}
			}
		}
	}
}

#endif // WITH_FREETYPE

#if WITH_HARFBUZZ

void FSlateTextShaper::PerformHarfBuzzTextShaping(const TCHAR* InText, const int32 InTextStart, const int32 InTextLen, const FSlateFontInfo &InFontInfo, const float InFontScale, const TextBiDi::ETextDirection InTextDirection, TArray<FShapedGlyphEntry>& OutGlyphsToRender) const
{
	// HarfBuzz can only shape data that uses the same font face, reads in the same direction, and uses the same script so we need to split the given text...
	TArray<FHarfBuzzTextSequenceEntry> HarfBuzzTextSequence;
	hb_unicode_funcs_t* HarfBuzzUnicodeFuncs = hb_unicode_funcs_get_default();

	// Step 1) Split the text into sections that are using the same font face (composite fonts may contain different faces for different character ranges)
	{
		// Data used while detecting font face boundaries
		int32 SplitStartIndex = InTextStart;
		int32 RunningTextIndex = InTextStart;
		const FFontData* RunningFontDataPtr = nullptr;
		TSharedPtr<FFreeTypeFace> RunningFaceAndMemory;
		float RunningSubFontScalingFactor = 1.0f;

		auto AppendPendingFontDataToSequence = [&]()
		{
			if (RunningFontDataPtr)
			{
				HarfBuzzTextSequence.Emplace(
					SplitStartIndex,						// InTextStartIndex
					RunningTextIndex - SplitStartIndex,		// InTextLength
					RunningFontDataPtr,						// InFontDataPtr
					RunningFaceAndMemory,					// InFaceAndMemory
					RunningSubFontScalingFactor				// InSubFontScalingFactor
					);

				RunningFontDataPtr = nullptr;
				RunningFaceAndMemory.Reset();
				RunningSubFontScalingFactor = 1.0f;
			}
		};

		const int32 TextEndIndex = InTextStart + InTextLen;
		for (; RunningTextIndex < TextEndIndex; ++RunningTextIndex)
		{
			float SubFontScalingFactor = 1.0f;
			const FFontData& FontData = CompositeFontCache->GetFontDataForCharacter(InFontInfo, InText[RunningTextIndex], SubFontScalingFactor);
			const FFreeTypeFaceGlyphData FaceGlyphData = FontRenderer->GetFontFaceForCharacter(FontData, InText[RunningTextIndex], EFontFallback::FF_Max);

			if (!RunningFontDataPtr || RunningFontDataPtr != &FontData || RunningFaceAndMemory != FaceGlyphData.FaceAndMemory || RunningSubFontScalingFactor != SubFontScalingFactor)
			{
				AppendPendingFontDataToSequence();

				SplitStartIndex = RunningTextIndex;
				RunningFontDataPtr = &FontData;
				RunningFaceAndMemory = FaceGlyphData.FaceAndMemory;
				RunningSubFontScalingFactor = SubFontScalingFactor;
			}
		}

		AppendPendingFontDataToSequence();
	}

	// Step 2) Split the font face sections by their their script code
	for (FHarfBuzzTextSequenceEntry& HarfBuzzTextSequenceEntry : HarfBuzzTextSequence)
	{
		// Data used while detecting script code boundaries
		int32 SplitStartIndex = HarfBuzzTextSequenceEntry.TextStartIndex;
		int32 RunningTextIndex = HarfBuzzTextSequenceEntry.TextStartIndex;
		TOptional<hb_script_t> RunningHarfBuzzScript;

		auto StartNewPendingTextSequence = [&](const hb_script_t InHarfBuzzScript)
		{
			SplitStartIndex = RunningTextIndex;
			RunningHarfBuzzScript = InHarfBuzzScript;
		};

		auto AppendPendingTextToSequence = [&]()
		{
			if (RunningHarfBuzzScript.IsSet())
			{
				HarfBuzzTextSequenceEntry.SubSequence.Emplace(
					SplitStartIndex,					// InStartIndex
					RunningTextIndex - SplitStartIndex,	// InLength
					RunningHarfBuzzScript.GetValue()	// InHarfBuzzScript
					);

				RunningHarfBuzzScript.Reset();
			}
		};

		auto IsSpecialCharacter = [](const hb_script_t InHarfBuzzScript) -> bool
		{
			// Characters in the common, inherited, and unknown scripts are allowed (and in the case of inherited, required) to merge with the script 
			// of the character(s) that preceded them. This also helps to minimize shaping batches, as spaces are within the common script.
			return InHarfBuzzScript == HB_SCRIPT_COMMON || InHarfBuzzScript == HB_SCRIPT_INHERITED || InHarfBuzzScript == HB_SCRIPT_UNKNOWN;
		};

		const int32 TextEndIndex = HarfBuzzTextSequenceEntry.TextStartIndex + HarfBuzzTextSequenceEntry.TextLength;
		for (; RunningTextIndex < TextEndIndex; ++RunningTextIndex)
		{
			const hb_script_t CharHarfBuzzScript = hb_unicode_script(HarfBuzzUnicodeFuncs, InText[RunningTextIndex]);				

			if (!RunningHarfBuzzScript.IsSet() || RunningHarfBuzzScript.GetValue() != CharHarfBuzzScript)
			{
				if (!RunningHarfBuzzScript.IsSet())
				{
					// Always start a new run if we're currently un-set
					StartNewPendingTextSequence(CharHarfBuzzScript);
				}
				else if (!IsSpecialCharacter(CharHarfBuzzScript))
				{
					if (IsSpecialCharacter(RunningHarfBuzzScript.GetValue()))
					{
						// If we started our run on a special character, we need to swap the script type to the non-special type as soon as we can
						RunningHarfBuzzScript = CharHarfBuzzScript;
					}
					else
					{
						// Transitioned a non-special character; end the current run and create a new one
						AppendPendingTextToSequence();
						StartNewPendingTextSequence(CharHarfBuzzScript);
					}
				}
			}
		}

		AppendPendingTextToSequence();
	}

	// Step 3) Now we use HarfBuzz to shape each font data sequence using its FreeType glyph
	{
		hb_buffer_t* HarfBuzzTextBuffer = hb_buffer_create();

		for (const FHarfBuzzTextSequenceEntry& HarfBuzzTextSequenceEntry : HarfBuzzTextSequence)
		{
			const float FinalFontScale = InFontScale * HarfBuzzTextSequenceEntry.SubFontScalingFactor;

			uint32 GlyphFlags = 0;

			const bool bHasKerning = FontRenderer->HasKerning(*HarfBuzzTextSequenceEntry.FontDataPtr);
			const hb_feature_t HarfBuzzFeatures[] = {
				{ HB_TAG('k','e','r','n'), bHasKerning, 0, uint32(-1) }
			};
			const int32 HarfBuzzFeaturesCount = ARRAY_COUNT(HarfBuzzFeatures);

			TSharedRef<FShapedGlyphFaceData> ShapedGlyphFaceData = MakeShareable(new FShapedGlyphFaceData(HarfBuzzTextSequenceEntry.FaceAndMemory, GlyphFlags, InFontInfo.Size, FinalFontScale));

			hb_font_t* HarfBuzzFont = HarfBuzzFontFactory.CreateFont(*HarfBuzzTextSequenceEntry.FaceAndMemory, GlyphFlags, InFontInfo.Size, FinalFontScale);

			for (const FHarfBuzzTextSequenceEntry::FSubSequenceEntry& HarfBuzzTextSubSequenceEntry : HarfBuzzTextSequenceEntry.SubSequence)
			{
				hb_buffer_set_cluster_level(HarfBuzzTextBuffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
				hb_buffer_set_direction(HarfBuzzTextBuffer, (InTextDirection == TextBiDi::ETextDirection::LeftToRight) ? HB_DIRECTION_LTR : HB_DIRECTION_RTL);
				hb_buffer_set_script(HarfBuzzTextBuffer, HarfBuzzTextSubSequenceEntry.HarfBuzzScript);

				HarfBuzzUtils::AppendStringToBuffer(InText, HarfBuzzTextSubSequenceEntry.StartIndex, HarfBuzzTextSubSequenceEntry.Length, HarfBuzzTextBuffer);
				hb_shape(HarfBuzzFont, HarfBuzzTextBuffer, HarfBuzzFeatures, HarfBuzzFeaturesCount);

				uint32 HarfBuzzGlyphCount = 0;
				hb_glyph_info_t* HarfBuzzGlyphInfos = hb_buffer_get_glyph_infos(HarfBuzzTextBuffer, &HarfBuzzGlyphCount);
				hb_glyph_position_t* HarfBuzzGlyphPositions = hb_buffer_get_glyph_positions(HarfBuzzTextBuffer, &HarfBuzzGlyphCount);

				OutGlyphsToRender.Reserve(OutGlyphsToRender.Num() + static_cast<int32>(HarfBuzzGlyphCount));
				for (uint32 HarfBuzzGlyphIndex = 0; HarfBuzzGlyphIndex < HarfBuzzGlyphCount; ++HarfBuzzGlyphIndex)
				{
					const hb_glyph_info_t& HarfBuzzGlyphInfo = HarfBuzzGlyphInfos[HarfBuzzGlyphIndex];
					const hb_glyph_position_t& HarfBuzzGlyphPosition = HarfBuzzGlyphPositions[HarfBuzzGlyphIndex];

					const int32 CurrentGlyphEntryIndex = OutGlyphsToRender.AddDefaulted();
					FShapedGlyphEntry& ShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex];
					ShapedGlyphEntry.FontFaceData = ShapedGlyphFaceData;
					ShapedGlyphEntry.GlyphIndex = HarfBuzzGlyphInfo.codepoint;
					ShapedGlyphEntry.ClusterIndex = static_cast<int32>(HarfBuzzGlyphInfo.cluster);
					ShapedGlyphEntry.XAdvance = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(HarfBuzzGlyphPosition.x_advance);
					ShapedGlyphEntry.YAdvance = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(HarfBuzzGlyphPosition.y_advance);
					ShapedGlyphEntry.XOffset = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(HarfBuzzGlyphPosition.x_offset);
					ShapedGlyphEntry.YOffset = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int16>(HarfBuzzGlyphPosition.y_offset);
					ShapedGlyphEntry.Kerning = 0;

					// Apply the kerning against the previous entry
					if (CurrentGlyphEntryIndex > 0 && bHasKerning)
					{
						FShapedGlyphEntry& PreviousShapedGlyphEntry = OutGlyphsToRender[CurrentGlyphEntryIndex - 1];

#if WITH_FREETYPE
						FT_Vector KerningVector;
						if (FTKerningPairCache->FindOrCache(HarfBuzzTextSequenceEntry.FaceAndMemory->GetFace(), FFreeTypeKerningPairCache::FKerningPair(PreviousShapedGlyphEntry.GlyphIndex, ShapedGlyphEntry.GlyphIndex), FT_KERNING_DEFAULT, InFontInfo.Size, FinalFontScale, KerningVector))
						{
							PreviousShapedGlyphEntry.Kerning = FreeTypeUtils::Convert26Dot6ToRoundedPixel<int8>(KerningVector.x);
						}
#endif // WITH_FREETYPE
					}
				}

				hb_buffer_clear_contents(HarfBuzzTextBuffer);
			}

			hb_font_destroy(HarfBuzzFont);
		}

		hb_buffer_destroy(HarfBuzzTextBuffer);
	}
}

#endif // WITH_HARFBUZZ
