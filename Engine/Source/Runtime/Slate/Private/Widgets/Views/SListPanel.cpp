// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
 
#include "SlatePrivatePCH.h"
#include "SListPanel.h"


FNoChildren SListPanel::NoChildren = FNoChildren();

SListPanel::SListPanel()
: Children()
{
}

/**
 * Construct the widget
 *
 * @param InArgs   A declaration from which to construct the widget
 */
void SListPanel::Construct( const FArguments& InArgs )
{
	PreferredRowNum = 0;
	SmoothScrollOffsetInItems = 0;
	OverscrollAmount = 0;
	ItemWidth = InArgs._ItemWidth;
	ItemHeight = InArgs._ItemHeight;
	NumDesiredItems = InArgs._NumDesiredItems;
	ItemAlignment = InArgs._ItemAlignment;
	bIsRefreshPending = false;
}

/** Make a new ListPanel::Slot  */
SListPanel::FSlot& SListPanel::Slot()
{
	return *(new FSlot());
}
	
/** Add a slot to the ListPanel */
SListPanel::FSlot& SListPanel::AddSlot(int32 InsertAtIndex)
{
	FSlot& NewSlot = SListPanel::Slot();
	if (InsertAtIndex == INDEX_NONE)
	{
		this->Children.Add( &NewSlot );
	}
	else
	{
		this->Children.Insert( &NewSlot, InsertAtIndex );
	}
	
	return NewSlot;
}
	
/**
 * Arrange the children top-to-bottom with not additional layout info.
 */
void SListPanel::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	if ( ShouldArrangeHorizontally() )
	{
		const EListItemAlignment ListItemAlignment = ItemAlignment.Get();

		// This is a tile view list, arrange items horizontally until there is no more room then create a new row.
		const float AllottedWidth = AllottedGeometry.Size.X;
		const float ItemPadding = GetItemPadding(AllottedGeometry, ListItemAlignment);
		const float HalfItemPadding = ItemPadding * 0.5;

		const float LocalItemWidth = GetItemWidth(AllottedGeometry, ListItemAlignment);
		const float LocalItemHeight = GetItemHeight();

		float WidthSoFar = 0;
		float HeightSoFar = -FMath::FloorToInt(SmoothScrollOffsetInItems * LocalItemHeight) - OverscrollAmount;

		bool bIsNewLine = true;
		for( int32 ItemIndex = 0; ItemIndex < Children.Num(); ++ItemIndex )
		{
			if ( bIsNewLine )
			{
				if ( ListItemAlignment == EListItemAlignment::RightAligned || ListItemAlignment == EListItemAlignment::CenterAligned )
				{
					const float LinePadding = GetLinePadding(AllottedGeometry, ItemIndex);
					if ( ListItemAlignment == EListItemAlignment::RightAligned )
					{
						WidthSoFar += LinePadding;
					}
					else
					{
						const float HalfLinePadding = LinePadding * 0.5;
						WidthSoFar += HalfLinePadding;
					}
				}

				bIsNewLine = false;
			}

			ArrangedChildren.AddWidget(
				AllottedGeometry.MakeChild(Children[ItemIndex].GetWidget(), FVector2D(WidthSoFar + HalfItemPadding, HeightSoFar), FVector2D(LocalItemWidth, LocalItemHeight))
				);
		
			WidthSoFar += LocalItemWidth + ItemPadding;

			if ( WidthSoFar + LocalItemWidth + ItemPadding > AllottedWidth )
			{
				WidthSoFar = 0;
				HeightSoFar += LocalItemHeight;
				bIsNewLine = true;
			}
		}
	}
	else
	{
		if (Children.Num() > 0)
		{
			// This is a normal list, arrange items vertically
			float HeightSoFar = -FMath::FloorToInt(SmoothScrollOffsetInItems * Children[0].GetWidget()->GetDesiredSize().Y)-OverscrollAmount;
			for( int32 ItemIndex = 0; ItemIndex < Children.Num(); ++ItemIndex )
			{
					const FVector2D ItemDesiredSize = Children[ItemIndex].GetWidget()->GetDesiredSize();
					const float LocalItemHeight = ItemDesiredSize.Y;

				// Note that ListPanel does not respect child Visibility.
				// It is simply not useful for ListPanels.
				ArrangedChildren.AddWidget(
					AllottedGeometry.MakeChild( Children[ItemIndex].GetWidget(), FVector2D(0, HeightSoFar), FVector2D(AllottedGeometry.Size.X, LocalItemHeight) )
					);

				HeightSoFar += LocalItemHeight;
			}
		}
	}
}
	
void SListPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (ShouldArrangeHorizontally())
	{
		const EListItemAlignment ListItemAlignment = ItemAlignment.Get();
		const float AllottedWidth = AllottedGeometry.Size.X;
		const float ItemPadding = GetItemPadding(AllottedGeometry, ListItemAlignment);
		const float LocalItemWidth = GetItemWidth(AllottedGeometry, ListItemAlignment);
		const float TotalItemSize = LocalItemWidth + ItemPadding;
		const int32 NumChildren = NumDesiredItems.Get();

		if (TotalItemSize > 0.0f && NumChildren > 0)
		{
			int32 NumColumns = FMath::Clamp(FMath::CeilToInt(AllottedWidth / TotalItemSize) - 1, 1, NumChildren);
			PreferredRowNum = FMath::CeilToInt(NumChildren / (float)NumColumns);
		}
		else
		{
			PreferredRowNum = 1;
		}
	}
	else
	{
		PreferredRowNum = NumDesiredItems.Get();
	}

}
	
/**
 * Simply the sum of all the children (vertically), and the largest width (horizontally).
 */
FVector2D SListPanel::ComputeDesiredSize( float ) const
{
	float MaxWidth = 0;
	float TotalHeight = 0;
	for( int32 ItemIndex = 0; ItemIndex < Children.Num(); ++ItemIndex )
	{
		// Notice that we do not respect child Visibility.
		// It is simply not useful for ListPanels.
		FVector2D ChildDesiredSize = Children[ItemIndex].GetWidget()->GetDesiredSize();
		MaxWidth = FMath::Max( ChildDesiredSize.X, MaxWidth );
		TotalHeight += ChildDesiredSize.Y;
	}

	if (ShouldArrangeHorizontally())
	{
		return FVector2D( MaxWidth, ItemHeight.Get() * PreferredRowNum );
	}
	else
	{
		return (Children.Num() > 0)
			? FVector2D( MaxWidth, TotalHeight/Children.Num()*PreferredRowNum )
			: FVector2D::ZeroVector;
	}
	
}

/**
 * @return A slot-agnostic representation of this panel's children
 */
FChildren* SListPanel::GetChildren()
{
	if (bIsRefreshPending)
	{
		// When a refresh is pending it is unsafe to cache the desired sizes of our children because
		// they may be representing unsound data structures. Any delegates/attributes accessing unsound
		// data will cause a crash.
		return &NoChildren;
	}
	else
	{
		return &Children;
	}
	
}

/** Set the offset of the view area from the top of the list. */
void SListPanel::SmoothScrollOffset(float InOffsetInItems)
{
	SmoothScrollOffsetInItems = InOffsetInItems;
}

void SListPanel::SetOverscrollAmount( float InOverscrollAmount )
{
	OverscrollAmount = InOverscrollAmount;
}

/** Remove all the children from this panel */
void SListPanel::ClearItems()
{
	Children.Empty();
}

float SListPanel::GetDesiredItemWidth() const
{
	return ItemWidth.Get();
}

float SListPanel::GetItemPadding(const FGeometry& AllottedGeometry) const
{
	return GetItemPadding(AllottedGeometry, ItemAlignment.Get());
}

float SListPanel::GetItemPadding(const FGeometry& AllottedGeometry, const EListItemAlignment ListItemAlignment) const
{
	const float LocalItemWidth = GetDesiredItemWidth();
	const int32 NumItemsWide = LocalItemWidth > 0 ? FMath::FloorToInt(AllottedGeometry.Size.X / LocalItemWidth) : 0;
	const bool IsEvenlyDistributedAlignment = ListItemAlignment == EListItemAlignment::EvenlyDistributed;

	// Only add padding between items if we have more total items that we can fit on a single row.  Otherwise,
	// the padding around items would continue to change proportionately with the (ample) free horizontal space
	float Padding = 0.0f;
	if (IsEvenlyDistributedAlignment && NumItemsWide > 0 && Children.Num() > NumItemsWide)
	{
		// Subtract a tiny amount from the available width to avoid floating point precision problems when arranging children
		static const float FloatingPointPrecisionOffset = 0.001f;

		Padding = (AllottedGeometry.Size.X - FloatingPointPrecisionOffset - NumItemsWide * LocalItemWidth) / NumItemsWide;
	}

	return Padding;
}

float SListPanel::GetItemWidth(const FGeometry& AllottedGeometry) const
{
	return GetItemWidth(AllottedGeometry, ItemAlignment.Get());
}

float SListPanel::GetItemWidth(const FGeometry& AllottedGeometry, const EListItemAlignment ListItemAlignment) const
{
	const float DesiredWidth = GetDesiredItemWidth();
	const int32 NumItemsWide = DesiredWidth > 0 ? FMath::Min(Children.Num(), FMath::FloorToInt(AllottedGeometry.Size.X / DesiredWidth)) : 0;
	const bool IsFillAlignment = ListItemAlignment == EListItemAlignment::Fill;

	float ExtraWidth = 0.0f;
	if (IsFillAlignment && NumItemsWide > 0)
	{
		// Subtract a tiny amount from the available width to avoid floating point precision problems when arranging children
		static const float FloatingPointPrecisionOffset = 0.001f;

		ExtraWidth = (AllottedGeometry.Size.X - FloatingPointPrecisionOffset - NumItemsWide * DesiredWidth) / NumItemsWide;
	}

	return DesiredWidth + ExtraWidth;
}

float SListPanel::GetLinePadding(const FGeometry& AllottedGeometry, const int32 LineStartIndex) const
{
	const int32 NumItemsLeft = Children.Num() - LineStartIndex;
	if(NumItemsLeft <= 0)
	{
		return 0.0f;
	}

	const float LocalItemWidth = GetItemWidth(AllottedGeometry);
	const int32 NumItemsWide = LocalItemWidth > 0 ? FMath::FloorToInt(AllottedGeometry.Size.X / LocalItemWidth) : 0;
	const int32 NumItemsOnLine = FMath::Min(NumItemsLeft, NumItemsWide);

	// Subtract a tiny amount from the available width to avoid floating point precision problems when arranging children
	static const float FloatingPointPrecisionOffset = 0.001f;

	return (AllottedGeometry.Size.X - FloatingPointPrecisionOffset - NumItemsOnLine * LocalItemWidth);
}

/** @return the uniform item height used when arranging children. */
float SListPanel::GetItemHeight() const
{
	return ItemHeight.Get();
}

void SListPanel::SetRefreshPending( bool IsPendingRefresh )
{
	bIsRefreshPending = IsPendingRefresh;
}

bool SListPanel::IsRefreshPending() const
{
	return bIsRefreshPending;
}

bool SListPanel::ShouldArrangeHorizontally() const
{
	return GetDesiredItemWidth() > 0;
}

void SListPanel::SetItemHeight(TAttribute<float> Height)
{
	ItemHeight = Height;
}

void SListPanel::SetItemWidth(TAttribute<float> Width)
{
	ItemWidth = Width;
}
