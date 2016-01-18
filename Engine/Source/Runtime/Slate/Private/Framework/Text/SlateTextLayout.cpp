// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"

#if WITH_FANCY_TEXT

#include "SlateTextLayout.h"

TSharedRef< FSlateTextLayout > FSlateTextLayout::Create(FTextBlockStyle InDefaultTextStyle)
{
	TSharedRef< FSlateTextLayout > Layout = MakeShareable( new FSlateTextLayout(MoveTemp(InDefaultTextStyle)) );
	Layout->AggregateChildren();

	return Layout;
}

FSlateTextLayout::FSlateTextLayout(FTextBlockStyle InDefaultTextStyle)
	: Children()
	, DefaultTextStyle(MoveTemp(InDefaultTextStyle))
{

}

FChildren* FSlateTextLayout::GetChildren()
{
	return &Children;
}

void FSlateTextLayout::ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	for (int32 LineIndex = 0; LineIndex < LineViews.Num(); LineIndex++)
	{
		const FTextLayout::FLineView& LineView = LineViews[ LineIndex ];

		for (int32 BlockIndex = 0; BlockIndex < LineView.Blocks.Num(); BlockIndex++)
		{
			const TSharedRef< ILayoutBlock > Block = LineView.Blocks[ BlockIndex ];
			const TSharedRef< ISlateRun > Run = StaticCastSharedRef< ISlateRun >( Block->GetRun() );
			Run->ArrangeChildren( Block, AllottedGeometry, ArrangedChildren );
		}
	}
}

int32 FSlateTextLayout::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FSlateRect ClippingRect = AllottedGeometry.GetClippingRect().IntersectionWith(MyClippingRect);
	const ESlateDrawEffect::Type DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	static bool ShowDebug = false;
	FLinearColor BlockHue( 0, 1.0f, 1.0f, 0.5 );
	int32 HighestLayerId = LayerId;

	for (const FTextLayout::FLineView& LineView : LineViews)
	{
		// Is this line visible?
		const FSlateRect LineViewRect(AllottedGeometry.AbsolutePosition + LineView.Offset, AllottedGeometry.AbsolutePosition + LineView.Offset + LineView.Size);
		const FSlateRect VisibleLineView = ClippingRect.IntersectionWith(LineViewRect);
		if (VisibleLineView.IsEmpty())
		{
			continue;
		}

		// Render any underlays for this line
		const int32 HighestUnderlayLayerId = OnPaintHighlights( Args, LineView, LineView.UnderlayHighlights, DefaultTextStyle, AllottedGeometry, ClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

		const int32 BlockDebugLayer = HighestUnderlayLayerId;
		const int32 TextLayer = BlockDebugLayer + 1;
		int32 HighestBlockLayerId = TextLayer;

		// Render every block for this line
		for (const TSharedRef< ILayoutBlock >& Block : LineView.Blocks)
		{
			if ( ShowDebug )
			{
				BlockHue.R += 50.0f;

				// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
				const float InverseScale = Inverse(AllottedGeometry.Scale);

				FSlateDrawElement::MakeBox(
					OutDrawElements, 
					BlockDebugLayer,
					AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Block->GetSize()), FSlateLayoutTransform(TransformPoint(InverseScale, Block->GetLocationOffset()))),
					&DefaultTextStyle.HighlightShape,
					ClippingRect,
					DrawEffects,
					InWidgetStyle.GetColorAndOpacityTint() * BlockHue.HSVToLinearRGB()
					);
			}

			const TSharedRef< ISlateRun > Run = StaticCastSharedRef< ISlateRun >( Block->GetRun() );

			int32 HighestRunLayerId = TextLayer;
			const TSharedPtr< ISlateRunRenderer > RunRenderer = StaticCastSharedPtr< ISlateRunRenderer >( Block->GetRenderer() );
			if ( RunRenderer.IsValid() )
			{
				HighestRunLayerId = RunRenderer->OnPaint( Args, LineView, Run, Block, DefaultTextStyle, AllottedGeometry, ClippingRect, OutDrawElements, TextLayer, InWidgetStyle, bParentEnabled );
			}
			else
			{
				HighestRunLayerId = Run->OnPaint( Args, LineView, Block, DefaultTextStyle, AllottedGeometry, ClippingRect, OutDrawElements, TextLayer, InWidgetStyle, bParentEnabled );
			}

			HighestBlockLayerId = FMath::Max( HighestBlockLayerId, HighestRunLayerId );
		}

		// Render any overlays for this line
		const int32 HighestOverlayLayerId = OnPaintHighlights( Args, LineView, LineView.OverlayHighlights, DefaultTextStyle, AllottedGeometry, ClippingRect, OutDrawElements, HighestBlockLayerId, InWidgetStyle, bParentEnabled );
		HighestLayerId = FMath::Max( HighestLayerId, HighestOverlayLayerId );
	}

	return HighestLayerId;
}

int32 FSlateTextLayout::OnPaintHighlights( const FPaintArgs& Args, const FTextLayout::FLineView& LineView, const TArray<FLineViewHighlight>& Highlights, const FTextBlockStyle& InDefaultTextStyle, const FGeometry& AllottedGeometry, const FSlateRect& ClippingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 CurrentLayerId = LayerId;

	for (const FLineViewHighlight& Highlight : Highlights)
	{
		const TSharedPtr< ISlateLineHighlighter > LineHighlighter = StaticCastSharedPtr< ISlateLineHighlighter >( Highlight.Highlighter );
		if (LineHighlighter.IsValid())
		{
			CurrentLayerId = LineHighlighter->OnPaint( Args, LineView, Highlight.OffsetX, Highlight.Width, InDefaultTextStyle, AllottedGeometry, ClippingRect, OutDrawElements, CurrentLayerId, InWidgetStyle, bParentEnabled );
		}
	}

	return CurrentLayerId;
}

void FSlateTextLayout::EndLayout()
{
	FTextLayout::EndLayout();
	AggregateChildren();
}

void FSlateTextLayout::SetDefaultTextStyle(FTextBlockStyle InDefaultTextStyle)
{
	DefaultTextStyle = MoveTemp(InDefaultTextStyle);
}

const FTextBlockStyle& FSlateTextLayout::GetDefaultTextStyle() const
{
	return DefaultTextStyle;
}

void FSlateTextLayout::AggregateChildren()
{
	Children.Empty();
	const TArray< FLineModel >& LayoutLineModels = GetLineModels();
	for (int32 LineModelIndex = 0; LineModelIndex < LayoutLineModels.Num(); LineModelIndex++)
	{
		const FLineModel& LineModel = LayoutLineModels[ LineModelIndex ];
		for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
		{
			const FRunModel& LineRun = LineModel.Runs[ RunIndex ];
			const TSharedRef< ISlateRun > SlateRun = StaticCastSharedRef< ISlateRun >( LineRun.GetRun() );

			const TArray< TSharedRef<SWidget> >& RunChildren = SlateRun->GetChildren();
			for (int32 ChildIndex = 0; ChildIndex < RunChildren.Num(); ChildIndex++)
			{
				const TSharedRef< SWidget >& Child = RunChildren[ ChildIndex ];
				Children.Add( Child );
			}
		}
	}
}

TSharedRef<IRun> FSlateTextLayout::CreateDefaultTextRun(const TSharedRef<FString>& NewText, const FTextRange& NewRange) const
{
	return FSlateTextRun::Create(FRunInfo(), NewText, DefaultTextStyle, NewRange);
}

#endif //WITH_FANCY_TEXT
