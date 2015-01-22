// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenHistoryView.cpp: Implements the SScreenHistoryView class.
=============================================================================*/
#include "ScreenShotComparisonPrivatePCH.h"


void SScreenHistoryView::Construct( const FArguments& InArgs )
{
	// Initialize some data
	SliderPostion = 0.0f;
	FirstItemToShow = 1; // The first item is locked in another column. Only show from 1 and up

	// The screen shot data to show
	ScreenShotData = InArgs._ScreenShotData;
	
	// Create the scroll box. We don't want to recreate this when we regenerate the other widgets as it will get reset
	TArray<TSharedPtr<IScreenShotData> >& ChildItems = ScreenShotData->GetFilteredChildren();

	// Only show scroll bar if we have more than 1 screenshot
	if ( ChildItems.Num() > 1 )
	{
		SAssignNew( ScrollBox, SBox )
			[
				SNew(SSlider)
				.Value( this, &SScreenHistoryView::GetSliderPosition )
				.OnValueChanged( this, &SScreenHistoryView::SetSliderPosition )
			];
	}

	ChildSlot
	[
		// Box to hold the generated widgets
		SAssignNew(WidgetBox, SVerticalBox)
	];

	RegenerateWidgets();
}


float SScreenHistoryView::GetSliderPosition() const
{
	// The position to put the slider in
	return SliderPostion;
}


void SScreenHistoryView::RegenerateWidgets()
{
	// Clear the box before regenerating the widgets
	WidgetBox->ClearChildren();
	TSharedPtr< SHorizontalBox > HolderBox;

	// Box where all widgets go
	WidgetBox->AddSlot()
	.AutoHeight()
	.VAlign( VAlign_Center )
	.HAlign( HAlign_Left )
	[
		SAssignNew( HolderBox, SHorizontalBox )
	];


	// Get the filtered views
	TArray<TSharedPtr<IScreenShotData> >& DevicesArray = ScreenShotData->GetFilteredChildren();

	// Create a new widget for each screen shot
	for ( int32 ChildSlots = FirstItemToShow; ChildSlots < DevicesArray.Num(); ChildSlots++ )
	{
		HolderBox->AddSlot()
		.AutoWidth()
		.VAlign( VAlign_Center )
		.HAlign( HAlign_Center )
		[
			SNew( SScreenShotItem )
			.ScreenShotData( DevicesArray[ChildSlots] )
		];
	}

	// Only show scroll bar if we have more than 1 screenshot
	if ( DevicesArray.Num() > 1 )
	{
		// Add the cached scrollbar back to the created widgets
		WidgetBox->AddSlot()
			.AutoHeight()
			.Padding(8, 0)
			[
				ScrollBox.ToSharedRef()
			];
	}
}


void SScreenHistoryView::SetSliderPosition( float NewPostion )
{
	SliderPostion = NewPostion;

	// Clamp the first item to show between 1 and the total number of views
	TArray<TSharedPtr<IScreenShotData> >& ChildItems = ScreenShotData->GetFilteredChildren();
	if ( ChildItems.Num() > 1 )
	{
		int32 NewItemToShow = FMath::Clamp( (int32)(( ChildItems.Num() * NewPostion) +1), 1, ChildItems.Num() -1);
		if ( NewItemToShow != FirstItemToShow )
		{
			FirstItemToShow = NewItemToShow;
			// Regenerate widgets
			RegenerateWidgets();
		}
	}
}

