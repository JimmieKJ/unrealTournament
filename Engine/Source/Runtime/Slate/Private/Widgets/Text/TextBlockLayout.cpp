// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "TextBlockLayout.h"
#include "SlateTextLayout.h"
#include "BaseTextLayoutMarshaller.h"
#include "SlateTextHighlightRunRenderer.h"

#if WITH_FANCY_TEXT

TSharedRef<FTextBlockLayout> FTextBlockLayout::Create(FTextBlockStyle InDefaultTextStyle, TSharedRef<FBaseTextLayoutMarshaller> InMarshaller, TSharedPtr<IBreakIterator> InLineBreakPolicy)
{
	return MakeShareable(new FTextBlockLayout(MoveTemp(InDefaultTextStyle), InMarshaller, InLineBreakPolicy));
}

FTextBlockLayout::FTextBlockLayout(FTextBlockStyle InDefaultTextStyle, TSharedRef<FBaseTextLayoutMarshaller> InMarshaller, TSharedPtr<IBreakIterator> InLineBreakPolicy)
	: TextLayout(FSlateTextLayout::Create(MoveTemp(InDefaultTextStyle)))
	, Marshaller(InMarshaller)
	, TextHighlighter(FSlateTextHighlightRunRenderer::Create())
	, CachedSize(ForceInitToZero)
	, CachedDesiredSize(ForceInitToZero)
{
	TextLayout->SetLineBreakIterator(InLineBreakPolicy);
}

FVector2D FTextBlockLayout::ComputeDesiredSize(const FWidgetArgs& InWidgetArgs/*, const float InScale*/, const FTextBlockStyle& InTextStyle)
{
	TextLayout->SetWrappingWidth(CalculateWrappingWidth(InWidgetArgs));
	TextLayout->SetMargin(InWidgetArgs.Margin.Get());
	TextLayout->SetJustification(InWidgetArgs.Justification.Get());
	TextLayout->SetLineHeightPercentage(InWidgetArgs.LineHeightPercentage.Get());

	// Has the style used for this text block changed?
	if(!IsStyleUpToDate(InTextStyle))
	{
		TextLayout->SetDefaultTextStyle(InTextStyle);
		Marshaller->MakeDirty(); // will regenerate the text using the new default style
	}

	{
		bool bRequiresTextUpdate = false;
		const FText& TextToSet = InWidgetArgs.Text.Get(FText::GetEmpty());
		if(!TextLastUpdate.IdenticalTo(TextToSet))
		{
			// The pointer used by the bound text has changed, however the text may still be the same - check that now
			if(!TextLastUpdate.IsDisplayStringEqualTo(TextToSet))
			{
				// The source text has changed, so update the internal text
				bRequiresTextUpdate = true;
			}

			// Update this even if the text is lexically identical, as it will update the pointer compared by IdenticalTo for the next Tick
			TextLastUpdate = FTextSnapshot(TextToSet);
		}

		if(bRequiresTextUpdate || Marshaller->IsDirty())
		{
			UpdateTextLayout(TextToSet);
		}
	}

	{
		const FText& HighlightTextToSet = InWidgetArgs.HighlightText.Get(FText::GetEmpty());
		if(!HighlightTextLastUpdate.IdenticalTo(HighlightTextToSet))
		{
			// The pointer used by the bound text has changed, however the text may still be the same - check that now
			if(!HighlightTextLastUpdate.IsDisplayStringEqualTo(HighlightTextToSet))
			{
				UpdateTextHighlights(HighlightTextToSet);
			}

			// Update this even if the text is lexically identical, as it will update the pointer compared by IdenticalTo for the next Tick
			HighlightTextLastUpdate = FTextSnapshot(HighlightTextToSet);
		}
	}

	// We need to update our cached desired size if the text layout has become dirty
	// todo: jdale - This is a hack until we can perform accurate measuring in ComputeDesiredSize
	if(TextLayout->IsLayoutDirty())
	{
		// The desired size must always have a scale of 1, OnPaint will make sure the scale is set correctly for painting
		TextLayout->SetScale(1.0f);
		TextLayout->UpdateIfNeeded();

		CachedDesiredSize = TextLayout->GetSize();
	}
	else
	{
		// This logic may look odd, but IsLayoutDirty() only checks that we've made a change that might affect the layout (which in turn might affect the 
		// desired size), however there's also highlight changes, which don't affect the size, but still require a call to UpdateIfNeeded() to be applied
		TextLayout->UpdateIfNeeded();
	}

	return CachedDesiredSize;
}

int32 FTextBlockLayout::OnPaint(const FPaintArgs& InPaintArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled)
{
	CachedSize = InAllottedGeometry.Size;

	TextLayout->SetScale(InAllottedGeometry.Scale);
	TextLayout->SetVisibleRegion(InAllottedGeometry.Size, FVector2D::ZeroVector);

	TextLayout->UpdateIfNeeded();

	return TextLayout->OnPaint(InPaintArgs, InAllottedGeometry, InClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void FTextBlockLayout::DirtyLayout()
{
	TextLayout->DirtyLayout();
}

FChildren* FTextBlockLayout::GetChildren()
{
	return TextLayout->GetChildren();
}

void FTextBlockLayout::ArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	TextLayout->ArrangeChildren(AllottedGeometry, ArrangedChildren);
}

void FTextBlockLayout::UpdateTextLayout(const FText& InText)
{
	Marshaller->ClearDirty();
	TextLayout->ClearLines();
	Marshaller->SetText(InText.ToString(), *TextLayout);

	TextLayout->ClearLineHighlights();
	TextLayout->ClearRunRenderers();

	HighlightTextLastUpdate = FTextSnapshot();
}

void FTextBlockLayout::UpdateTextHighlights(const FText& InHighlightText)
{
	const FString& HighlightTextString = InHighlightText.ToString();
	const int32 HighlightTextLength = HighlightTextString.Len();

	const TArray< FTextLayout::FLineModel >& LineModels = TextLayout->GetLineModels();

	TArray<FTextRunRenderer> TextHighlights;
	for(int32 LineIndex = 0; LineIndex < LineModels.Num(); ++LineIndex)
	{
		const FTextLayout::FLineModel& LineModel = LineModels[LineIndex];

		int32 FindBegin = 0;
		int32 CurrentHighlightBegin;
		const int32 TextLength = LineModel.Text->Len();
		while(FindBegin < TextLength && (CurrentHighlightBegin = LineModel.Text->Find(HighlightTextString, ESearchCase::IgnoreCase, ESearchDir::FromStart, FindBegin)) != INDEX_NONE)
		{
			FindBegin = CurrentHighlightBegin + HighlightTextLength;

			if(TextHighlights.Num() > 0 && TextHighlights.Last().LineIndex == LineIndex && TextHighlights.Last().Range.EndIndex == CurrentHighlightBegin)
			{
				TextHighlights[TextHighlights.Num() - 1] = FTextRunRenderer(LineIndex, FTextRange(TextHighlights.Last().Range.BeginIndex, FindBegin), TextHighlighter.ToSharedRef());
			}
			else
			{
				TextHighlights.Add(FTextRunRenderer(LineIndex, FTextRange(CurrentHighlightBegin, FindBegin), TextHighlighter.ToSharedRef()));
			}
		}
	}

	TextLayout->SetRunRenderers(TextHighlights);
}

bool FTextBlockLayout::IsStyleUpToDate(const FTextBlockStyle& NewStyle) const
{
	const FTextBlockStyle& CurrentStyle = TextLayout->GetDefaultTextStyle();

	return (CurrentStyle.Font == NewStyle.Font)
		&& (CurrentStyle.ColorAndOpacity == NewStyle.ColorAndOpacity)
		&& (CurrentStyle.ShadowOffset == NewStyle.ShadowOffset)
		&& (CurrentStyle.ShadowColorAndOpacity == NewStyle.ShadowColorAndOpacity)
		&& (CurrentStyle.SelectedBackgroundColor == NewStyle.SelectedBackgroundColor)
		&& (CurrentStyle.HighlightColor == NewStyle.HighlightColor)
		&& (CurrentStyle.HighlightShape == NewStyle.HighlightShape);
}

float FTextBlockLayout::CalculateWrappingWidth(const FWidgetArgs& InWidgetArgs) const
{
	const float WrapTextAt = InWidgetArgs.WrapTextAt.Get(0.0f);
	const bool bAutoWrapText = InWidgetArgs.AutoWrapText.Get(false);

	// Text wrapping can either be used defined (WrapTextAt), automatic (bAutoWrapText and CachedSize), 
	// or a mixture of both. Take whichever has the smallest value (>1)
	float WrappingWidth = WrapTextAt;
	if(bAutoWrapText && CachedSize.X >= 1.0f)
	{
		WrappingWidth = (WrappingWidth >= 1.0f) ? FMath::Min(WrappingWidth, CachedSize.X) : CachedSize.X;
	}

	return FMath::Max(0.0f, WrappingWidth);
}

#endif //WITH_FANCY_TEXT
