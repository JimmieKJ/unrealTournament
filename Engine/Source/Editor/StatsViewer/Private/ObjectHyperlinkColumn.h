// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IPropertyTableCustomColumn.h"
#include "StatsViewerModule.h"
#include "ObjectHyperlinkColumnInitializationOptions.h"

/**
 * A property table custom column used to display names of objects
 * that can be clicked on to jump to the objects in the scene or 
 * content browser.
 */
class FObjectHyperlinkColumn : public IPropertyTableCustomColumn
{
public:
	FObjectHyperlinkColumn(const FObjectHyperlinkColumnInitializationOptions& InOptions = FObjectHyperlinkColumnInitializationOptions());

	/** Begin IPropertyTableCustomColumn interface */
	virtual bool Supports( const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities ) const override;
	virtual TSharedPtr< SWidget > CreateColumnLabel( const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style ) const override;
	virtual TSharedPtr< IPropertyTableCellPresenter > CreateCellPresenter( const TSharedRef< IPropertyTableCell >& Cell, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style ) const override;
	/** End IPropertyTableCustomColumn interface */

private:

	/** Delegate called to generate a custom widget */
	FOnGenerateWidget OnGenerateWidget;

	/** Delegate called when a hyperlink is clicked */
	FOnObjectHyperlinkClicked OnObjectHyperlinkClicked;

	/** Delegate used to query whether a UClass is supported */
	FOnIsClassSupported OnIsClassSupported;
};

