// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "FontCacheFreeType.h"
#include "FontCacheHarfBuzz.h"
#include "FontCacheCompositeFont.h"
#include "SlateFontRenderer.h"
#include "SlateTextShaper.h"
#include "LegacySlateFontInfoCache.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Font Atlases"), STAT_SlateNumFontAtlases, STATGROUP_SlateMemory);
DECLARE_MEMORY_STAT(TEXT("Font Kerning Table Memory"), STAT_SlateFontKerningTableMemory, STATGROUP_SlateMemory);
DECLARE_MEMORY_STAT(TEXT("Shaped Glyph Sequence Memory"), STAT_SlateShapedGlyphSequenceMemory, STATGROUP_SlateMemory);
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


FShapedGlyphSequence::FShapedGlyphSequence(TArray<FShapedGlyphEntry> InGlyphsToRender, TArray<FShapedGlyphClusterBlock> InGlyphClusterBlocks, const int16 InTextBaseline, const uint16 InMaxTextHeight, const UObject* InFontMaterial, const FSourceTextRange& InSourceTextRange)
	: GlyphsToRender(MoveTemp(InGlyphsToRender))
	, GlyphClusterBlocks(MoveTemp(InGlyphClusterBlocks))
	, TextBaseline(InTextBaseline)
	, MaxTextHeight(InMaxTextHeight)
	, FontMaterial(InFontMaterial)
	, SequenceWidth(0)
	, GlyphFontFaces()
	, ClusterIndicesToGlyphData(InSourceTextRange)
{
	for (int32 CurrentClusterBlockIndex = 0; CurrentClusterBlockIndex < GlyphClusterBlocks.Num(); ++CurrentClusterBlockIndex)
	{
		const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[CurrentClusterBlockIndex];

		for (int32 CurrentGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex; CurrentGlyphIndex < CurrentClusterBlock.ShapedGlyphEndIndex; ++CurrentGlyphIndex)
		{
			const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];

			// Track unique font faces
			GlyphFontFaces.AddUnique(CurrentGlyph.FontFaceData->FontFace);

			// Update the measured width
			SequenceWidth += CurrentGlyph.XAdvance;

			// Track reverse look-up data
			FClusterIndexToGlyphData* ClusterIndexToGlyphData = ClusterIndicesToGlyphData.GetGlyphData(CurrentGlyph.ClusterIndex);
			checkSlow(ClusterIndexToGlyphData);
			if (ClusterIndexToGlyphData->IsValid())
			{
				// If this data already exists then it means a single character produced multiple glyphs and we need to track it as an additional glyph (these are always within the same cluster block)
				ClusterIndexToGlyphData->AdditionalGlyphIndices.Add(CurrentGlyphIndex);
			}
			else
			{
				*ClusterIndexToGlyphData = FClusterIndexToGlyphData(CurrentClusterBlockIndex, CurrentGlyphIndex);
			}
		}
	}

	// Track memory usage
	INC_MEMORY_STAT_BY(STAT_SlateShapedGlyphSequenceMemory, GetAllocatedSize());
}

FShapedGlyphSequence::~FShapedGlyphSequence()
{
	// Untrack memory usage
	DEC_MEMORY_STAT_BY(STAT_SlateShapedGlyphSequenceMemory, GetAllocatedSize());
}

uint32 FShapedGlyphSequence::GetAllocatedSize() const
{
	return GlyphsToRender.GetAllocatedSize() + GlyphClusterBlocks.GetAllocatedSize() + GlyphFontFaces.GetAllocatedSize() + ClusterIndicesToGlyphData.GetAllocatedSize();
}

bool FShapedGlyphSequence::IsDirty() const
{
	for (const auto& GlyphFontFace : GlyphFontFaces)
	{
		if (!GlyphFontFace.IsValid())
		{
			return true;
		}
	}

	return false;
}

int32 FShapedGlyphSequence::GetMeasuredWidth() const
{
	return SequenceWidth;
}

TOptional<int32> FShapedGlyphSequence::GetMeasuredWidth(const int32 InStartIndex, const int32 InEndIndex, const bool InIncludeKerningWithPrecedingGlyph) const
{
	int32 MeasuredWidth = 0;

	if (InIncludeKerningWithPrecedingGlyph && InStartIndex > 0)
	{
		const TOptional<int8> Kerning = GetKerning(InStartIndex - 1);
		MeasuredWidth += Kerning.Get(0);
	}

	auto GlyphCallback = [&](const FShapedGlyphEntry& CurrentGlyph)
	{
		MeasuredWidth += CurrentGlyph.XAdvance;
	};

	if (EnumerateGlyphsInClusterRange(InStartIndex, InEndIndex, GlyphCallback))
	{
		return MeasuredWidth;
	}

	return TOptional<int32>();
}

FShapedGlyphSequence::FGlyphOffsetResult FShapedGlyphSequence::GetGlyphAtOffset(FSlateFontCache& InFontCache, const int32 InHorizontalOffset) const
{
	if (GlyphsToRender.Num() == 0)
	{
		return FGlyphOffsetResult();
	}

	int32 CurrentOffset = 0;
	int32 MatchedGlyphIndex = INDEX_NONE;
	TextBiDi::ETextDirection MatchedGlyphTextDirection = TextBiDi::ETextDirection::LeftToRight;

	int32 PreviousGlyphIndex = INDEX_NONE;
	for (const FShapedGlyphClusterBlock& CurrentClusterBlock : GlyphClusterBlocks)
	{
		// Measure all the in-range glyphs from this cluster block
		for (int32 CurrentGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex; CurrentGlyphIndex < CurrentClusterBlock.ShapedGlyphEndIndex; ++CurrentGlyphIndex)
		{
			const FShapedGlyphEntry* CurrentGlyphPtr = &GlyphsToRender[CurrentGlyphIndex];
			const int32 FirstGlyphIndexInGlyphCluster = CurrentGlyphIndex;

			// A single character may produce multiple glyphs which must be treated as a single logic unit
			int32 TotalGlyphSpacing = 0;
			for (;;)
			{
				const FShapedGlyphFontAtlasData GlyphAtlasData = InFontCache.GetShapedGlyphFontAtlasData(*CurrentGlyphPtr);
				TotalGlyphSpacing += GlyphAtlasData.HorizontalOffset + CurrentGlyphPtr->XAdvance;

				const bool bIsWithinGlyphCluster = GlyphsToRender.IsValidIndex(CurrentGlyphIndex + 1) && CurrentGlyphPtr->ClusterIndex == GlyphsToRender[CurrentGlyphIndex + 1].ClusterIndex;
				if (!bIsWithinGlyphCluster)
				{
					break;
				}

				CurrentGlyphPtr = &GlyphsToRender[++CurrentGlyphIndex];
			}

			// Round our test toward the glyphs center position based on the reading direction of the text
			const int32 GlyphWidthToTest = (CurrentGlyphPtr->NumCharactersInGlyph > 1) ? TotalGlyphSpacing : TotalGlyphSpacing / 2;

			if (InHorizontalOffset < (CurrentOffset + GlyphWidthToTest))
			{
				if (CurrentClusterBlock.TextDirection == TextBiDi::ETextDirection::LeftToRight)
				{
					MatchedGlyphIndex = FirstGlyphIndexInGlyphCluster;
				}
				else
				{
					// Right-to-left text needs to return the previous glyph index, since that is the logical "next" glyph
					if (PreviousGlyphIndex == INDEX_NONE)
					{
						// No previous glyph, so use the end index of the current cluster block since that will draw to the left of this glyph, and will trigger the cursor to move to the opposite side of the current glyph
						return FGlyphOffsetResult(CurrentClusterBlock.ClusterEndIndex);
					}
					else
					{
						MatchedGlyphIndex = PreviousGlyphIndex;
					}
				}
				MatchedGlyphTextDirection = CurrentClusterBlock.TextDirection;
				break;
			}

			PreviousGlyphIndex = FirstGlyphIndexInGlyphCluster;

			CurrentOffset += CurrentGlyphPtr->XAdvance;
		}
	}

	if (MatchedGlyphIndex == INDEX_NONE)
	{
		// The offset was outside of the current text, so say we hit the rightmost cluster from the last cluster block
		const FShapedGlyphClusterBlock& LastClusterBlock = GlyphClusterBlocks.Last();
		return FGlyphOffsetResult((LastClusterBlock.TextDirection == TextBiDi::ETextDirection::LeftToRight) ? LastClusterBlock.ClusterEndIndex : LastClusterBlock.ClusterStartIndex);
	}

	const FShapedGlyphEntry& MatchedGlyph = GlyphsToRender[MatchedGlyphIndex];
	return FGlyphOffsetResult(&MatchedGlyph, MatchedGlyphTextDirection, CurrentOffset);
}

TOptional<FShapedGlyphSequence::FGlyphOffsetResult> FShapedGlyphSequence::GetGlyphAtOffset(FSlateFontCache& InFontCache, int32 InStartIndex, int32 InEndIndex, const int32 InHorizontalOffset, const bool InIncludeKerningWithPrecedingGlyph) const
{
	int32 CurrentOffset = 0;
	int32 VisuallyRightmostClusterIndex = INDEX_NONE;
	int32 MatchedGlyphIndex = INDEX_NONE;
	TextBiDi::ETextDirection MatchedGlyphTextDirection = TextBiDi::ETextDirection::LeftToRight;

	bool bFoundStartGlyph = false;
	bool bFoundEndGlyph = false;

	if (InIncludeKerningWithPrecedingGlyph && InStartIndex > 0)
	{
		const TOptional<int8> Kerning = GetKerning(InStartIndex - 1);
		CurrentOffset += Kerning.Get(0);
	}

	// Try and work out which cluster block we should start measuring from
	int32 CurrentClusterBlockIndex = 0;
	{
		// The given range is exclusive, but we use an inclusive range when finding the cluster block to start at
		const FClusterIndexToGlyphData* StartClusterIndexToGlyphData = ClusterIndicesToGlyphData.GetGlyphData(InStartIndex);
		const FClusterIndexToGlyphData* EndClusterIndexToGlyphData = ClusterIndicesToGlyphData.GetGlyphData(InEndIndex - 1);

		if (StartClusterIndexToGlyphData && StartClusterIndexToGlyphData->IsValid() && EndClusterIndexToGlyphData && EndClusterIndexToGlyphData->IsValid())
		{
			CurrentClusterBlockIndex = FMath::Min(StartClusterIndexToGlyphData->ClusterBlockIndex, EndClusterIndexToGlyphData->ClusterBlockIndex);
		}
	}

	const TRange<int32> SearchRange(InStartIndex, InEndIndex);
	int32 PreviousGlyphIndex = INDEX_NONE;
	for (; CurrentClusterBlockIndex < GlyphClusterBlocks.Num(); ++CurrentClusterBlockIndex)
	{
		const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[CurrentClusterBlockIndex];

		const TRange<int32> CurrentClusterBlockRange(CurrentClusterBlock.ClusterStartIndex, CurrentClusterBlock.ClusterEndIndex);
		if (CurrentClusterBlockRange.Overlaps(SearchRange))
		{
			// Measure all the in-range glyphs from this cluster block
			for (int32 CurrentGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex; CurrentGlyphIndex < CurrentClusterBlock.ShapedGlyphEndIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry* CurrentGlyphPtr = &GlyphsToRender[CurrentGlyphIndex];
				const int32 FirstGlyphIndexInGlyphCluster = CurrentGlyphIndex;

				if (!bFoundStartGlyph && CurrentGlyphPtr->ClusterIndex == InStartIndex)
				{
					bFoundStartGlyph = true;
				}

				if (!bFoundEndGlyph && CurrentGlyphPtr->ClusterIndex == InEndIndex)
				{
					bFoundEndGlyph = true;
				}

				if (CurrentGlyphPtr->ClusterIndex >= InStartIndex && CurrentGlyphPtr->ClusterIndex < InEndIndex)
				{
					// A single character may produce multiple glyphs which must be treated as a single logic unit
					int32 TotalGlyphSpacing = 0;
					for (;;)
					{
						const FShapedGlyphFontAtlasData GlyphAtlasData = InFontCache.GetShapedGlyphFontAtlasData(*CurrentGlyphPtr);
						TotalGlyphSpacing += GlyphAtlasData.HorizontalOffset + CurrentGlyphPtr->XAdvance;

						const bool bIsWithinGlyphCluster = GlyphsToRender.IsValidIndex(CurrentGlyphIndex + 1) && CurrentGlyphPtr->ClusterIndex == GlyphsToRender[CurrentGlyphIndex + 1].ClusterIndex;
						if (!bIsWithinGlyphCluster)
						{
							break;
						}

						CurrentGlyphPtr = &GlyphsToRender[++CurrentGlyphIndex];
					}

					// Round our test toward the glyphs center position based on the reading direction of the text
					const int32 GlyphWidthToTest = (CurrentGlyphPtr->NumCharactersInGlyph > 1) ? TotalGlyphSpacing : TotalGlyphSpacing / 2;

					// Round our test toward the glyphs center position based on the reading direction of the text
					if (InHorizontalOffset < (CurrentOffset + GlyphWidthToTest))
					{
						if (CurrentClusterBlock.TextDirection == TextBiDi::ETextDirection::LeftToRight)
						{
							MatchedGlyphIndex = CurrentGlyphIndex;
						}
						else
						{
							// Right-to-left text needs to return the previous glyph index, since that is the logical "next" glyph
							if (PreviousGlyphIndex == INDEX_NONE)
							{
								// No previous glyph, so use the end index of the current cluster block since that will draw to the left of this glyph, and will trigger the cursor to move to the opposite side of the current glyph
								return FGlyphOffsetResult(FMath::Min(InEndIndex, CurrentClusterBlock.ClusterEndIndex));
							}
							else
							{
								MatchedGlyphIndex = PreviousGlyphIndex;
							}
						}
						MatchedGlyphTextDirection = CurrentClusterBlock.TextDirection;
						break;
					}

					CurrentOffset += CurrentGlyphPtr->XAdvance;
				}

				PreviousGlyphIndex = FirstGlyphIndexInGlyphCluster;

				// If we found both our end-points, we can bail now
				if (bFoundStartGlyph && bFoundEndGlyph)
				{
					break;
				}
			}

			// Update VisuallyRightmostClusterIndex from this cluster based on the text direction
			VisuallyRightmostClusterIndex = (CurrentClusterBlock.TextDirection == TextBiDi::ETextDirection::LeftToRight)
				? FMath::Min(InEndIndex, CurrentClusterBlock.ClusterEndIndex)
				: FMath::Max(InStartIndex, CurrentClusterBlock.ClusterStartIndex);

			// The shaped glyphs don't contain the end cluster block index, so if we matched end of the cluster range, say we measured okay
			if (CurrentClusterBlock.ClusterEndIndex == InEndIndex)
			{
				bFoundEndGlyph = true;
			}
		}

		if (MatchedGlyphIndex != INDEX_NONE)
		{
			break;
		}
	}

	if (MatchedGlyphIndex == INDEX_NONE && bFoundEndGlyph)
	{
		// The offset was outside of the current text, so say we hit the rightmost cluster that was within range
		return FGlyphOffsetResult(VisuallyRightmostClusterIndex);
	}

	// Did we measure okay?
	if (bFoundStartGlyph && MatchedGlyphIndex != INDEX_NONE)
	{
		const FShapedGlyphEntry& MatchedGlyph = GlyphsToRender[MatchedGlyphIndex];
		return FGlyphOffsetResult(&MatchedGlyph, MatchedGlyphTextDirection, CurrentOffset);
	}

	return TOptional<FGlyphOffsetResult>();
}

TOptional<int8> FShapedGlyphSequence::GetKerning(const int32 InIndex) const
{
	const FClusterIndexToGlyphData* ClusterIndexToGlyphData = ClusterIndicesToGlyphData.GetGlyphData(InIndex);
	if (ClusterIndexToGlyphData && ClusterIndexToGlyphData->IsValid())
	{
		const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[ClusterIndexToGlyphData->GlyphIndex];
		checkSlow(CurrentGlyph.ClusterIndex == InIndex);
		return CurrentGlyph.Kerning;
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

	int32 CurrentSubClusterBlockIndex = INDEX_NONE;

	auto BeginClusterBlock = [&](const FShapedGlyphClusterBlock& CurrentClusterBlock)
	{
		checkSlow(CurrentSubClusterBlockIndex == INDEX_NONE);
		CurrentSubClusterBlockIndex = SubGlyphClusterBlocks.AddDefaulted();

		FShapedGlyphClusterBlock& SubClusterBlock = SubGlyphClusterBlocks[CurrentSubClusterBlockIndex];
		SubClusterBlock = FShapedGlyphClusterBlock(CurrentClusterBlock.TextDirection, FMath::Max(CurrentClusterBlock.ClusterStartIndex, InStartIndex), FMath::Min(CurrentClusterBlock.ClusterEndIndex, InEndIndex));

		SubClusterBlock.ShapedGlyphStartIndex = SubGlyphsToRender.Num();
	};

	auto EndClusterBlock = [&](const FShapedGlyphClusterBlock& CurrentClusterBlock)
	{
		FShapedGlyphClusterBlock& SubClusterBlock = SubGlyphClusterBlocks[CurrentSubClusterBlockIndex];
		SubClusterBlock.ShapedGlyphEndIndex = SubGlyphsToRender.Num();

		checkSlow(CurrentSubClusterBlockIndex != INDEX_NONE);
		CurrentSubClusterBlockIndex = INDEX_NONE;
	};

	auto GlyphCallback = [&](const FShapedGlyphEntry& CurrentGlyph)
	{
		SubGlyphsToRender.Add(CurrentGlyph);
	};

	if (EnumerateGlyphsInClusterRange(InStartIndex, InEndIndex, BeginClusterBlock, EndClusterBlock, GlyphCallback))
	{
		return MakeShareable(new FShapedGlyphSequence(MoveTemp(SubGlyphsToRender), MoveTemp(SubGlyphClusterBlocks), TextBaseline, MaxTextHeight, FontMaterial, FSourceTextRange(InStartIndex, InEndIndex - InStartIndex)));
	}

	return nullptr;
}

bool FShapedGlyphSequence::EnumerateGlyphsInClusterRange(const int32 InStartIndex, const int32 InEndIndex, const FForEachShapedGlyphEntryCallback& InGlyphCallback) const
{
	if (InStartIndex == InEndIndex)
	{
		// Nothing to enumerate, but don't say we failed
		return true;
	}

	// The given range is exclusive, but we use an inclusive range when performing all the bounds testing below (as it makes things simpler)
	const FClusterIndexToGlyphData* StartClusterIndexToGlyphData = ClusterIndicesToGlyphData.GetGlyphData(InStartIndex);
	const FClusterIndexToGlyphData* EndClusterIndexToGlyphData = ClusterIndicesToGlyphData.GetGlyphData(InEndIndex - 1);

	if (!(StartClusterIndexToGlyphData && StartClusterIndexToGlyphData->IsValid() && EndClusterIndexToGlyphData && EndClusterIndexToGlyphData->IsValid()))
	{
		return false;
	}

	if (StartClusterIndexToGlyphData->ClusterBlockIndex == EndClusterIndexToGlyphData->ClusterBlockIndex)
	{
		// The start and end point are within the same block - this is simple to enumerate
		const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[StartClusterIndexToGlyphData->ClusterBlockIndex];

		int32 StartGlyphIndex = INDEX_NONE;
		int32 EndGlyphIndex = INDEX_NONE;
		if (StartClusterIndexToGlyphData->GlyphIndex <= EndClusterIndexToGlyphData->GlyphIndex)
		{
			StartGlyphIndex = StartClusterIndexToGlyphData->GetLowestGlyphIndex();
			EndGlyphIndex = EndClusterIndexToGlyphData->GetHighestGlyphIndex();
		}
		else
		{
			StartGlyphIndex = EndClusterIndexToGlyphData->GetLowestGlyphIndex();
			EndGlyphIndex = StartClusterIndexToGlyphData->GetHighestGlyphIndex();
		}
		check(StartGlyphIndex <= EndGlyphIndex);

		for (int32 CurrentGlyphIndex = StartGlyphIndex; CurrentGlyphIndex <= EndGlyphIndex; ++CurrentGlyphIndex)
		{
			const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];
			InGlyphCallback(CurrentGlyph);
		}
	}
	else
	{
		if (EndClusterIndexToGlyphData->ClusterBlockIndex < StartClusterIndexToGlyphData->ClusterBlockIndex)
		{
			Swap(StartClusterIndexToGlyphData, EndClusterIndexToGlyphData);
		}

		// Enumerate everything from the start point in the start block
		{
			const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[StartClusterIndexToGlyphData->ClusterBlockIndex];

			int32 StartGlyphIndex = INDEX_NONE;
			int32 EndGlyphIndex = INDEX_NONE;
			if (StartClusterIndexToGlyphData->GlyphIndex <= (CurrentClusterBlock.ShapedGlyphEndIndex - 1))
			{
				StartGlyphIndex = StartClusterIndexToGlyphData->GetLowestGlyphIndex();
				EndGlyphIndex = CurrentClusterBlock.ShapedGlyphEndIndex - 1; // Inclusive bounds test in the loop below
			}
			else
			{
				StartGlyphIndex = CurrentClusterBlock.ShapedGlyphEndIndex - 1; // Inclusive bounds test in the loop below
				EndGlyphIndex = StartClusterIndexToGlyphData->GetHighestGlyphIndex();
			}
			check(StartGlyphIndex <= EndGlyphIndex);

			for (int32 CurrentGlyphIndex = StartGlyphIndex; CurrentGlyphIndex <= EndGlyphIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];
				InGlyphCallback(CurrentGlyph);
			}
		}

		// Enumerate everything in any middle blocks
		for (int32 CurrentClusterBlockIndex = StartClusterIndexToGlyphData->ClusterBlockIndex + 1; CurrentClusterBlockIndex <= EndClusterIndexToGlyphData->ClusterBlockIndex - 1; ++CurrentClusterBlockIndex)
		{
			const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[CurrentClusterBlockIndex];

			for (int32 CurrentGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex; CurrentGlyphIndex < CurrentClusterBlock.ShapedGlyphEndIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];
				InGlyphCallback(CurrentGlyph);
			}
		}

		// Enumerate everything to the end point in the end block
		{
			const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[EndClusterIndexToGlyphData->ClusterBlockIndex];

			int32 StartGlyphIndex = INDEX_NONE;
			int32 EndGlyphIndex = INDEX_NONE;
			if (CurrentClusterBlock.ShapedGlyphStartIndex <= EndClusterIndexToGlyphData->GlyphIndex)
			{
				StartGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex;
				EndGlyphIndex = EndClusterIndexToGlyphData->GetHighestGlyphIndex();
			}
			else
			{
				StartGlyphIndex = EndClusterIndexToGlyphData->GetLowestGlyphIndex();
				EndGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex;
			}
			check(StartGlyphIndex <= EndGlyphIndex);

			for (int32 CurrentGlyphIndex = StartGlyphIndex; CurrentGlyphIndex <= EndGlyphIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];
				InGlyphCallback(CurrentGlyph);
			}
		}
	}

	return true;
}

bool FShapedGlyphSequence::EnumerateGlyphsInClusterRange(const int32 InStartIndex, const int32 InEndIndex, const FForEachShapedGlyphClusterBlockCallback& InBeginClusterBlockCallback, const FForEachShapedGlyphClusterBlockCallback& InEndClusterBlockCallback, const FForEachShapedGlyphEntryCallback& InGlyphCallback) const
{
	if (InStartIndex == InEndIndex)
	{
		// Nothing to enumerate, but don't say we failed
		return true;
	}

	// The given range is exclusive, but we use an inclusive range when performing all the bounds testing below (as it makes things simpler)
	const FClusterIndexToGlyphData* StartClusterIndexToGlyphData = ClusterIndicesToGlyphData.GetGlyphData(InStartIndex);
	const FClusterIndexToGlyphData* EndClusterIndexToGlyphData = ClusterIndicesToGlyphData.GetGlyphData(InEndIndex - 1);

	if (!(StartClusterIndexToGlyphData && StartClusterIndexToGlyphData->IsValid() && EndClusterIndexToGlyphData && EndClusterIndexToGlyphData->IsValid()))
	{
		return false;
	}

	if (StartClusterIndexToGlyphData->ClusterBlockIndex == EndClusterIndexToGlyphData->ClusterBlockIndex)
	{
		// The start and end point are within the same block - this is simple to enumerate
		const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[StartClusterIndexToGlyphData->ClusterBlockIndex];

		int32 StartGlyphIndex = INDEX_NONE;
		int32 EndGlyphIndex = INDEX_NONE;
		if (StartClusterIndexToGlyphData->GlyphIndex <= EndClusterIndexToGlyphData->GlyphIndex)
		{
			StartGlyphIndex = StartClusterIndexToGlyphData->GetLowestGlyphIndex();
			EndGlyphIndex = EndClusterIndexToGlyphData->GetHighestGlyphIndex();
		}
		else
		{
			StartGlyphIndex = EndClusterIndexToGlyphData->GetLowestGlyphIndex();
			EndGlyphIndex = StartClusterIndexToGlyphData->GetHighestGlyphIndex();
		}
		check(StartGlyphIndex <= EndGlyphIndex);

		InBeginClusterBlockCallback(CurrentClusterBlock);
		for (int32 CurrentGlyphIndex = StartGlyphIndex; CurrentGlyphIndex <= EndGlyphIndex; ++CurrentGlyphIndex)
		{
			const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];
			InGlyphCallback(CurrentGlyph);
		}
		InEndClusterBlockCallback(CurrentClusterBlock);
	}
	else
	{
		if (EndClusterIndexToGlyphData->ClusterBlockIndex < StartClusterIndexToGlyphData->ClusterBlockIndex)
		{
			Swap(StartClusterIndexToGlyphData, EndClusterIndexToGlyphData);
		}

		// Enumerate everything from the start point in the start block
		{
			const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[StartClusterIndexToGlyphData->ClusterBlockIndex];

			int32 StartGlyphIndex = INDEX_NONE;
			int32 EndGlyphIndex = INDEX_NONE;
			if (StartClusterIndexToGlyphData->GlyphIndex <= (CurrentClusterBlock.ShapedGlyphEndIndex - 1))
			{
				StartGlyphIndex = StartClusterIndexToGlyphData->GetLowestGlyphIndex();
				EndGlyphIndex = CurrentClusterBlock.ShapedGlyphEndIndex - 1; // Inclusive bounds test in the loop below
			}
			else
			{
				StartGlyphIndex = CurrentClusterBlock.ShapedGlyphEndIndex - 1; // Inclusive bounds test in the loop below
				EndGlyphIndex = StartClusterIndexToGlyphData->GetHighestGlyphIndex();
			}
			check(StartGlyphIndex <= EndGlyphIndex);

			InBeginClusterBlockCallback(CurrentClusterBlock);
			for (int32 CurrentGlyphIndex = StartGlyphIndex; CurrentGlyphIndex <= EndGlyphIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];
				InGlyphCallback(CurrentGlyph);
			}
			InEndClusterBlockCallback(CurrentClusterBlock);
		}

		// Enumerate everything in any middle blocks
		for (int32 CurrentClusterBlockIndex = StartClusterIndexToGlyphData->ClusterBlockIndex + 1; CurrentClusterBlockIndex <= EndClusterIndexToGlyphData->ClusterBlockIndex - 1; ++CurrentClusterBlockIndex)
		{
			const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[CurrentClusterBlockIndex];

			InBeginClusterBlockCallback(CurrentClusterBlock);
			for (int32 CurrentGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex; CurrentGlyphIndex < CurrentClusterBlock.ShapedGlyphEndIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];
				InGlyphCallback(CurrentGlyph);
			}
			InEndClusterBlockCallback(CurrentClusterBlock);
		}

		// Enumerate everything to the end point in the end block
		{
			const FShapedGlyphClusterBlock& CurrentClusterBlock = GlyphClusterBlocks[EndClusterIndexToGlyphData->ClusterBlockIndex];

			int32 StartGlyphIndex = INDEX_NONE;
			int32 EndGlyphIndex = INDEX_NONE;
			if (CurrentClusterBlock.ShapedGlyphStartIndex <= EndClusterIndexToGlyphData->GlyphIndex)
			{
				StartGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex;
				EndGlyphIndex = EndClusterIndexToGlyphData->GetHighestGlyphIndex();
			}
			else
			{
				StartGlyphIndex = EndClusterIndexToGlyphData->GetLowestGlyphIndex();
				EndGlyphIndex = CurrentClusterBlock.ShapedGlyphStartIndex;
			}
			check(StartGlyphIndex <= EndGlyphIndex);

			InBeginClusterBlockCallback(CurrentClusterBlock);
			for (int32 CurrentGlyphIndex = StartGlyphIndex; CurrentGlyphIndex <= EndGlyphIndex; ++CurrentGlyphIndex)
			{
				const FShapedGlyphEntry& CurrentGlyph = GlyphsToRender[CurrentGlyphIndex];
				InGlyphCallback(CurrentGlyph);
			}
			InEndClusterBlockCallback(CurrentClusterBlock);
		}
	}

	return true;
}


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

int8 FCharacterList::GetKerning(TCHAR FirstChar, TCHAR SecondChar, const EFontFallback MaxFontFallback)
{
	const FCharacterEntry First = GetCharacter(FirstChar, MaxFontFallback);
	const FCharacterEntry Second = GetCharacter(SecondChar, MaxFontFallback);
	return GetKerning(First, Second);
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

bool FCharacterList::CanCacheCharacter(TCHAR Character, const EFontFallback MaxFontFallback)
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

		bReturnVal = FontCache.FontRenderer->CanLoadCharacter(FontData, Character, MaxFontFallback);
	}

	return bReturnVal;
}

FCharacterEntry FCharacterList::GetCharacter(TCHAR Character, const EFontFallback MaxFontFallback)
{
	const FCharacterEntry* ReturnVal = nullptr;
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

	if (ReturnVal)
	{
		bNeedCaching = !ReturnVal->IsCached();

		// If the character needs caching, but can't be cached, reject the character
		if (bNeedCaching && !CanCacheCharacter(Character, MaxFontFallback))
		{
			bNeedCaching = false;
			ReturnVal = nullptr;
		}
	}
	// Only map the character if it can be cached
	else if (CanCacheCharacter(Character, MaxFontFallback))
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


	if (ReturnVal)
	{
		if (bNeedCaching)
		{
			ReturnVal = &(CacheCharacter(Character));
		}
		// For already-cached characters, reject characters that don't fall within maximum font fallback level requirements
		else if (Character != SlateFontRendererUtils::InvalidSubChar && MaxFontFallback < ReturnVal->FallbackLevel)
		{
			ReturnVal = nullptr;
		}
	}

	if (ReturnVal)
	{
		return *ReturnVal;
	}

	// The character is not valid, replace with the invalid character substitute
	return GetCharacter(SlateFontRendererUtils::InvalidSubChar, MaxFontFallback);
}

const FCharacterEntry& FCharacterList::CacheCharacter( TCHAR Character )
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

	FInternationalization::Get().OnCultureChanged().AddRaw(this, &FSlateFontCache::HandleCultureChanged);
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

int32 FSlateFontCache::GetLocalizedFallbackFontRevision() const
{
	return FLegacySlateFontInfoCache::Get().GetLocalizedFallbackFontRevision();
}

void FSlateFontCache::RequestFlushCache()
{
	bFlushRequested = true;
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

void FSlateFontCache::FlushCompositeFont(const FCompositeFont& InCompositeFont)
{
	CompositeFontCache->FlushCompositeFont(InCompositeFont);
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

	FontToCharacterListCache.Empty();
	ShapedGlyphToAtlasData.Empty();
}

void FSlateFontCache::HandleCultureChanged()
{
	// The culture has changed, so request the font cache be flushed once it is safe to do so
	// We don't flush immediately as the request may come in from a different thread than the one that owns the font cache
	bFlushRequested = true;
}
