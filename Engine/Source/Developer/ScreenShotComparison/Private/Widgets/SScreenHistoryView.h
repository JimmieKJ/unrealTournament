// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenHistoryView.h: Declares the SScreenHistoryView class.
=============================================================================*/

#pragma once

class SScreenHistoryView : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SScreenHistoryView )
		: _ScreenShotData()
	{}

	SLATE_ARGUMENT( TSharedPtr<IScreenShotData>, ScreenShotData )

	SLATE_END_ARGS()

	/**
	 * Construct the widget.
	 *
	 * @param InArgs   A declaration from which to construct the widget.
 	 */
	void Construct( const FArguments& InArgs );

public:

	/**
	 * Get the slider position.
	 *
	 * @return The slider position.
 	 */
	float GetSliderPosition() const;

	/**
	 * Regenerate the widgets after the slider has changed.
 	 */
	void RegenerateWidgets();

	/**
	 * Sets the slider position.
	 *
 	 * @param The new slider position.
 	 */
	void SetSliderPosition( float NewPostion );

private:

	// Index of the fist item to show.
	int32 FirstItemToShow;

	// Holds the horizontal scrollbar widget.
	TSharedPtr< SBox > ScrollBox;

	// Holds the position of the slider bar.
	float SliderPostion;
	
	// Holds the item being edited.
	TSharedPtr<IScreenShotData> ScreenShotData;

	// Holds the widget that is filled with screen shot widgets.
	TSharedPtr< SVerticalBox > WidgetBox;
};

