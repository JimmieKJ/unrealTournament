// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlatePrivatePCH.h"
#include "MultiBox.h"
#include "SGroupMarkerBlock.h"

/**
 * Constructor
 */
FGroupStartBlock::FGroupStartBlock()
	: FMultiBlock( NULL, NULL )
{
}

/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FGroupStartBlock::ConstructWidget() const
{
	return SNew( SGroupMarkerBlock );
}

/**
 * Constructor
 */
FGroupEndBlock::FGroupEndBlock()
	: FMultiBlock( NULL, NULL )
{
}

/**
 * Allocates a widget for this type of MultiBlock.  Override this in derived classes.
 *
 * @return  MultiBlock widget object
 */
TSharedRef< class IMultiBlockBaseWidget > FGroupEndBlock::ConstructWidget() const
{
	return SNew( SGroupMarkerBlock );
}

/**
 * Builds this MultiBlock widget up from the MultiBlock associated with it,
 * in this case this is a blank block and therefore provides a null widget.
 */
void SGroupMarkerBlock::BuildMultiBlockWidget(const ISlateStyle* StyleSet, const FName& StyleName)
{
	ChildSlot
	[
		SNullWidget::NullWidget
	];
}

