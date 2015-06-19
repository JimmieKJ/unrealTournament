// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "LayoutUtils.h"

SWrapBox::SWrapBox()
: Slots()
{
}

SWrapBox::FSlot& SWrapBox::Slot()
{
	return *( new SWrapBox::FSlot() );
}

SWrapBox::FSlot& SWrapBox::AddSlot()
{
	SWrapBox::FSlot* NewSlot = new SWrapBox::FSlot();
	Slots.Add(NewSlot);
	return *NewSlot;
}

int32 SWrapBox::RemoveSlot( const TSharedRef<SWidget>& SlotWidget )
{
	for (int32 SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx)
	{
		if ( SlotWidget == Slots[SlotIdx].GetWidget() )
		{
			Slots.RemoveAt(SlotIdx);
			return SlotIdx;
		}
	}

	return -1;
}

void SWrapBox::Construct( const FArguments& InArgs )
{
	PreferredWidth = InArgs._PreferredWidth;
	InnerSlotPadding = InArgs._InnerSlotPadding;
	bUseAllottedWidth = InArgs._UseAllottedWidth;

	// Copy the children from the declaration to the widget
	for ( int32 ChildIndex=0; ChildIndex < InArgs.Slots.Num(); ++ChildIndex )
	{
		Slots.Add( InArgs.Slots[ChildIndex] );
	}
}

void SWrapBox::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bUseAllottedWidth)
	{
		PreferredWidth = AllottedGeometry.Size.X;
	}
}

void SWrapBox::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const float MyPreferredWidth = PreferredWidth.Get();

	// PREPASS
	// Figure out when we start wrapping and how tall each line (aka. row) needs to be
	TArray<int32, TInlineAllocator<3> > WidgetIndexStartsNewLine;
	TArray<float, TInlineAllocator<3> > LineHeights;
	{
		float WidthSoFar = 0;
		float LineHeightSoFar = 0;
		for ( int32 ChildIndex=0; ChildIndex < Slots.Num(); ++ChildIndex )
		{
			const FSlot& CurSlot = Slots[ChildIndex];
			const TSharedRef<SWidget>& ThisChild = CurSlot.GetWidget();

			if ( ThisChild->GetVisibility() == EVisibility::Collapsed )
			{
				continue;
			}

			// If slot isn't the first one on the line, we need to add the inner slot padding.
			const float ConditionPaddingX = ( WidthSoFar == 0 ) ? 0 : InnerSlotPadding.X;
			
			FVector2D ThisSlotSize = ThisChild->GetDesiredSize() + CurSlot.SlotPadding.Get().GetDesiredSize();

			// If the width is smaller than this slots fill limit, we need to account for it filling the line
			// and forcing all controls that follow it to wrap.
			if ( CurSlot.SlotFillLineWhenWidthLessThan.IsSet() )
			{
				if ( MyPreferredWidth < CurSlot.SlotFillLineWhenWidthLessThan.GetValue() )
				{
					ThisSlotSize.X = MyPreferredWidth;
				}
			}

			if ( ( WidthSoFar + ThisSlotSize.X + ConditionPaddingX ) > MyPreferredWidth )
			{
				WidgetIndexStartsNewLine.Add( ChildIndex );
				LineHeights.Add( LineHeightSoFar );
				WidthSoFar = ThisSlotSize.X + ConditionPaddingX;
				LineHeightSoFar = ThisSlotSize.Y;
			}
			else
			{
				WidthSoFar += ThisSlotSize.X;
				LineHeightSoFar = FMath::Max( ThisSlotSize.Y + InnerSlotPadding.Y, LineHeightSoFar );
			}
		}
		// Add trailing values so that we won't out of bound (easier than adding extra conditionals)
		LineHeights.Add(LineHeightSoFar);
		WidgetIndexStartsNewLine.Add(INDEX_NONE);
	}

	// ACTUALLY ARRANGE
	float XOffsetSoFar = 0;
	float YOffsetSoFar = 0;
	for ( int32 CurrentLineIndex=0, ChildIndex=0; ChildIndex < Slots.Num(); ++ChildIndex )
	{
		const FSlot& CurSlot = Slots[ChildIndex];
		const TSharedRef<SWidget>& ThisChild = CurSlot.GetWidget();

		if ( ThisChild->GetVisibility() == EVisibility::Collapsed )
		{
			continue;
		}

		// If slot isn't on the first line, we need to add the inner slot padding.
		const float ConditionPaddingY = ( YOffsetSoFar == 0 ) ? 0 : InnerSlotPadding.Y;

		// Do we need to start a new line?
		const bool bStartNewLine = ( WidgetIndexStartsNewLine[CurrentLineIndex] == ChildIndex );
		if (bStartNewLine)
		{
			// New line starts at the very left
			XOffsetSoFar = 0;
			// 
			YOffsetSoFar += LineHeights[CurrentLineIndex] + InnerSlotPadding.Y;

			// Move on to the next line.
			CurrentLineIndex++;
		}

		// If slot isn't the first one on the line, we need to add the inner slot padding.
		const float ConditionPaddingX = ( XOffsetSoFar == 0 ) ? 0 : InnerSlotPadding.X;

		// Is this the last widget on the line?
		bool bLastWidgetOnLine = ((ChildIndex + 1) >= Slots.Num()) || WidgetIndexStartsNewLine[CurrentLineIndex] == ( ChildIndex + 1 );

		const float CurrentLineHeight = LineHeights[ CurrentLineIndex ];
		const FMargin& SlotPadding(CurSlot.SlotPadding.Get());

		float ThisSlotSizeX = ThisChild->GetDesiredSize().X + SlotPadding.GetTotalSpaceAlong<Orient_Horizontal>();

		// If the width is smaller than this slots fill limit, we need to account for it filling the line
		// and forcing all controls that follow it to wrap.
		if ( CurSlot.SlotFillLineWhenWidthLessThan.IsSet() )
		{
			if ( MyPreferredWidth < CurSlot.SlotFillLineWhenWidthLessThan.GetValue() )
			{
				ThisSlotSizeX = MyPreferredWidth;
			}
		}

		// If this slot fills empty space, we need to have it fill the remaining space on the line.
		if ( bLastWidgetOnLine && CurSlot.bSlotFillEmptySpace )
		{
			ThisSlotSizeX = MyPreferredWidth - (XOffsetSoFar - ConditionPaddingX);
		}

		// Horizontal alignment will always be equivalent to HAlign_Fill due to the nature of the widget.
		// But we still need to account for padding.		
		AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>(ThisSlotSizeX, CurSlot, SlotPadding + ConditionPaddingX);
		AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>(CurrentLineHeight, CurSlot, SlotPadding + ConditionPaddingY);

		ArrangedChildren.AddWidget( AllottedGeometry.MakeChild(
			ThisChild,
			FVector2D( XOffsetSoFar + XAlignmentResult.Offset, YOffsetSoFar + YAlignmentResult.Offset),
			FVector2D( XAlignmentResult.Size, YAlignmentResult.Size )
		) );

		XOffsetSoFar += ThisSlotSizeX;		
	}
}

void SWrapBox::ClearChildren()
{
	Slots.Empty();
}

FVector2D SWrapBox::ComputeDesiredSize( float ) const
{
	FVector2D MyDesiredSize = FVector2D::ZeroVector;
	FVector2D RowDesiredSize = FVector2D::ZeroVector;

	const float MyPreferredWidth = PreferredWidth.Get();
	
	
	// Find the size of every row. Once the size of the row exceeds the preferred width,
	// start a new row. Whenever we start a new row, update the desired size of the entire widget.
	for ( int32 ChildIndex=0; ChildIndex < Slots.Num(); ++ChildIndex )
	{
		const FSlot& CurSlot = Slots[ChildIndex];
		const TSharedRef<SWidget>& ThisChild = CurSlot.GetWidget();

		if ( ThisChild->GetVisibility() == EVisibility::Collapsed )
		{
			continue;
		}

		// If slot isn't the first one on the line, we need to add the inner slot padding.
		const float ConditionPaddingX = ( RowDesiredSize.X == 0 ) ? 0 : InnerSlotPadding.X;

		FVector2D ThisChildDesiredSize = ThisChild->GetDesiredSize() + CurSlot.SlotPadding.Get().GetDesiredSize() + ConditionPaddingX;

		// If the width is smaller than this slots fill limit, we need to account for it filling the line
		// and forcing all controls that follow it to wrap.
		if ( CurSlot.SlotFillLineWhenWidthLessThan.IsSet() )
		{
			if ( MyPreferredWidth <CurSlot.SlotFillLineWhenWidthLessThan.GetValue() )
			{
				ThisChildDesiredSize.X = MyPreferredWidth;
			}
		}

		if ( (RowDesiredSize.X + ThisChildDesiredSize.X) > MyPreferredWidth)
		{
			// We are starting a new line; update the total desired size.
			MyDesiredSize.X = FMath::Max(RowDesiredSize.X, MyDesiredSize.X);
			MyDesiredSize.Y += RowDesiredSize.Y + InnerSlotPadding.Y;

			RowDesiredSize = FVector2D::ZeroVector;
		}

		RowDesiredSize.X += ThisChildDesiredSize.X;
		RowDesiredSize.Y = FMath::Max( RowDesiredSize.Y, ThisChildDesiredSize.Y );
	}

	// Update the desired size with the current unfinished line.
	MyDesiredSize.X = FMath::Max(RowDesiredSize.X, MyDesiredSize.X);
	MyDesiredSize.Y += RowDesiredSize.Y;

	return MyDesiredSize;
}

FChildren* SWrapBox::GetChildren()
{
	return &Slots;	
}

void SWrapBox::SetInnerSlotPadding(FVector2D InInnerSlotPadding)
{
	InnerSlotPadding = InInnerSlotPadding;
}
