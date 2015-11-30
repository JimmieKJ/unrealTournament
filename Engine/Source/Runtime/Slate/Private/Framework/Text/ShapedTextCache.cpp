// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "ShapedTextCache.h"

#if WITH_FANCY_TEXT

FShapedGlyphSequencePtr FShapedTextCache::FindShapedText(const FCachedShapedTextKey& InKey) const
{
	FShapedGlyphSequencePtr ShapedText = CachedShapedText.FindRef(InKey);

	if (ShapedText.IsValid() && !ShapedText->IsDirty())
	{
		return ShapedText;
	}
	
	return nullptr;
}

FShapedGlyphSequenceRef FShapedTextCache::AddShapedText(const FCachedShapedTextKey& InKey, const TCHAR* InText, const FSlateFontInfo& InFont)
{
	FShapedGlyphSequenceRef ShapedText = FontCache.ShapeBidirectionalText(
		InText, 
		InKey.TextRange.BeginIndex, 
		InKey.TextRange.Len(), 
		InFont, 
		InKey.Scale, 
		InKey.TextContext.BaseDirection, 
		InKey.TextContext.TextShapingMethod
		);

	return AddShapedText(InKey, ShapedText);
}

FShapedGlyphSequenceRef FShapedTextCache::AddShapedText(const FCachedShapedTextKey& InKey, const TCHAR* InText, const FSlateFontInfo& InFont, const TextBiDi::ETextDirection InTextDirection)
{
	FShapedGlyphSequenceRef ShapedText = FontCache.ShapeUnidirectionalText(
		InText, 
		InKey.TextRange.BeginIndex, 
		InKey.TextRange.Len(), 
		InFont, 
		InKey.Scale, 
		InTextDirection, 
		InKey.TextContext.TextShapingMethod
		);

	return AddShapedText(InKey, ShapedText);
}

FShapedGlyphSequenceRef FShapedTextCache::AddShapedText(const FCachedShapedTextKey& InKey, FShapedGlyphSequenceRef InShapedText)
{
	CachedShapedText.Add(InKey, InShapedText);
	return InShapedText;
}

FShapedGlyphSequenceRef FShapedTextCache::FindOrAddShapedText(const FCachedShapedTextKey& InKey, const TCHAR* InText, const FSlateFontInfo& InFont)
{
	FShapedGlyphSequencePtr ShapedText = FindShapedText(InKey);

	if (!ShapedText.IsValid())
	{
		ShapedText = AddShapedText(InKey, InText, InFont);
	}

	return ShapedText.ToSharedRef();
}

FShapedGlyphSequenceRef FShapedTextCache::FindOrAddShapedText(const FCachedShapedTextKey& InKey, const TCHAR* InText, const FSlateFontInfo& InFont, const TextBiDi::ETextDirection InTextDirection)
{
	FShapedGlyphSequencePtr ShapedText = FindShapedText(InKey);

	if (!ShapedText.IsValid())
	{
		ShapedText = AddShapedText(InKey, InText, InFont, InTextDirection);
	}

	return ShapedText.ToSharedRef();
}

void FShapedTextCache::Clear()
{
	CachedShapedText.Reset();
}


FVector2D ShapedTextCacheUtil::MeasureShapedText(const FShapedTextCacheRef& InShapedTextCache, const FCachedShapedTextKey& InRunKey, const FTextRange& InMeasureRange, const TCHAR* InText, const FSlateFontInfo& InFont)
{
	// Get the shaped text for the entire run and try and take a sub-measurement from it - this can help minimize the amount of text shaping that needs to be done when measuring text
	FShapedGlyphSequenceRef ShapedText = InShapedTextCache->FindOrAddShapedText(InRunKey, InText, InFont);

	TOptional<int32> MeasuredWidth = ShapedText->GetMeasuredWidth(InMeasureRange.BeginIndex, InMeasureRange.EndIndex);
	if (!MeasuredWidth.IsSet())
	{
		FCachedShapedTextKey MeasureKey = InRunKey;
		MeasureKey.TextRange = InMeasureRange;

		// Couldn't measure the sub-range, try and measure from a shape of the specified range
		ShapedText = InShapedTextCache->FindOrAddShapedText(MeasureKey, InText, InFont);
		MeasuredWidth = ShapedText->GetMeasuredWidth();
	}

	check(MeasuredWidth.IsSet());

	return FVector2D(MeasuredWidth.GetValue(), ShapedText->GetMaxTextHeight());
}

int8 ShapedTextCacheUtil::GetShapedGlyphKerning(const FShapedTextCacheRef& InShapedTextCache, const FCachedShapedTextKey& InRunKey, const int32 InGlyphIndex, const TCHAR* InText, const FSlateFontInfo& InFont)
{
	// Get the shaped text for the entire run and try and get the kerning from it - this can help minimize the amount of text shaping that needs to be done when calculating kerning
	FShapedGlyphSequenceRef ShapedText = InShapedTextCache->FindOrAddShapedText(InRunKey, InText, InFont);

	TOptional<int8> Kerning = ShapedText->GetKerning(InGlyphIndex);
	if (!Kerning.IsSet())
	{
		FCachedShapedTextKey KerningKey = InRunKey;
		KerningKey.TextRange = FTextRange(InGlyphIndex, InGlyphIndex + 2);

		// Couldn't get the kerning from the main run data, try and get the kerning from a shape of the specified range
		ShapedText = InShapedTextCache->FindOrAddShapedText(KerningKey, InText, InFont);
		Kerning = ShapedText->GetKerning(InGlyphIndex);
	}

	return Kerning.Get(0);
}

FShapedGlyphSequenceRef ShapedTextCacheUtil::GetShapedTextSubSequence(const FShapedTextCacheRef& InShapedTextCache, const FCachedShapedTextKey& InRunKey, const FTextRange& InTextRange, const TCHAR* InText, const FSlateFontInfo& InFont, const TextBiDi::ETextDirection InTextDirection)
{
	// Get the shaped text for the entire run and try and make a sub-sequence from it - this can help minimize the amount of text shaping that needs to be done when drawing text
	FShapedGlyphSequencePtr ShapedText = InShapedTextCache->FindOrAddShapedText(InRunKey, InText, InFont);

	if (InRunKey.TextRange != InTextRange)
	{
		FCachedShapedTextKey SubSequenceKey = InRunKey;
		SubSequenceKey.TextRange = InTextRange;

		// Do we already have a cached entry for this? We don't use FindOrAdd here as if it's missing, we first want to try and extract it from our run of shaped text
		FShapedGlyphSequencePtr FoundShapedText = InShapedTextCache->FindShapedText(SubSequenceKey);

		if (FoundShapedText.IsValid())
		{
			ShapedText = FoundShapedText;
		}
		else
		{
			// Didn't find it in the cache, so first try and extract a sub-sequence from the run of shaped text
			ShapedText = ShapedText->GetSubSequence(InTextRange.BeginIndex, InTextRange.EndIndex);

			if (ShapedText.IsValid())
			{
				// Add this new sub-sequence to the cache
				InShapedTextCache->AddShapedText(SubSequenceKey, ShapedText.ToSharedRef());
			}
			else
			{
				// Couldn't get the sub-sequence, try and make a new shape for it instead
				ShapedText = InShapedTextCache->FindOrAddShapedText(SubSequenceKey, InText, InFont, InTextDirection);
			}
		}
	}

	check(ShapedText.IsValid());

	return ShapedText.ToSharedRef();
}

#endif //WITH_FANCY_TEXT
