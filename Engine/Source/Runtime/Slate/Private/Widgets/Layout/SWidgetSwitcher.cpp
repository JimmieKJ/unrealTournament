// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "LayoutUtils.h"
#include "SWidgetSwitcher.h"


/* Local constants
 *****************************************************************************/

const TAttribute<FVector2D> ContentScale = FVector2D::UnitVector;


/* SWidgetSwitcher interface
 *****************************************************************************/

SWidgetSwitcher::SWidgetSwitcher()
	: AllChildren()
{ }


SWidgetSwitcher::FSlot& SWidgetSwitcher::AddSlot( int32 SlotIndex )
{
	FSlot* NewSlot = new FSlot();

	if (!AllChildren.IsValidIndex(SlotIndex))
	{
		AllChildren.Add(NewSlot);
	}
	else
	{
		AllChildren.Insert(NewSlot, SlotIndex);
	}

	return *NewSlot;
}


void SWidgetSwitcher::Construct( const FArguments& InArgs )
{
	OneDynamicChild = FOneDynamicChild( &AllChildren, &WidgetIndex );

	for (int32 Index = 0; Index < InArgs.Slots.Num(); ++Index)
	{
		AllChildren.Add(InArgs.Slots[Index]);
	}

	WidgetIndex = InArgs._WidgetIndex;
}


TSharedPtr<SWidget> SWidgetSwitcher::GetActiveWidget( ) const
{
	const int32 ActiveWidgetIndex = WidgetIndex.Get();
	if (ActiveWidgetIndex >= 0)
	{
		return AllChildren[ActiveWidgetIndex].GetWidget();
	}

	return nullptr;
}


TSharedPtr<SWidget> SWidgetSwitcher::GetWidget( int32 SlotIndex ) const
{
	if (AllChildren.IsValidIndex(SlotIndex))
	{
		return AllChildren[SlotIndex].GetWidget();
	}

	return nullptr;
}


int32 SWidgetSwitcher::GetWidgetIndex( TSharedRef<SWidget> Widget ) const
{
	for (int32 Index = 0; Index < AllChildren.Num(); ++Index)
	{
		const FSlot& Slot = AllChildren[Index];

		if (Slot.GetWidget() == Widget)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}


int32 SWidgetSwitcher::RemoveSlot( TSharedRef<SWidget> WidgetToRemove )
{
	for (int32 SlotIndex=0; SlotIndex < AllChildren.Num(); ++SlotIndex)
	{
		if (AllChildren[SlotIndex].GetWidget() == WidgetToRemove)
		{
			AllChildren.RemoveAt(SlotIndex);
			return SlotIndex;
		}
	}

	return -1;
}


void SWidgetSwitcher::SetActiveWidgetIndex( int32 Index )
{
	WidgetIndex = Index;
}


/* SCompoundWidget interface
 *****************************************************************************/

void SWidgetSwitcher::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	if (AllChildren.Num() > 0)
	{
		ArrangeSingleChild(AllottedGeometry, ArrangedChildren, AllChildren[WidgetIndex.Get()], ContentScale);
	}
}
	
FVector2D SWidgetSwitcher::ComputeDesiredSize( float ) const
{
	return AllChildren.Num() > 0
		? AllChildren[WidgetIndex.Get()].GetWidget()->GetDesiredSize()
		: FVector2D::ZeroVector;
}

FChildren* SWidgetSwitcher::GetChildren( )
{
	return &OneDynamicChild;
}
