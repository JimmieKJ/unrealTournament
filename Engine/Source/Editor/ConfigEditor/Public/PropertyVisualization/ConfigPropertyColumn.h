// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor/PropertyEditor/Public/IPropertyTableColumn.h"
#include "Editor/PropertyEditor/Public/IPropertyTableCustomColumn.h"


/**
* A property table custom column used to display source control condition of files.
*/
class CONFIGEDITOR_API FConfigPropertyCustomColumn : public IPropertyTableCustomColumn
{
public:
	FConfigPropertyCustomColumn(){}

	/** Begin IPropertyTableCustomColumn interface */
	virtual bool Supports(const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities) const override;
	virtual TSharedPtr< SWidget > CreateColumnLabel(const TSharedRef< IPropertyTableColumn >& Column, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style) const override;
	virtual TSharedPtr< IPropertyTableCellPresenter > CreateCellPresenter(const TSharedRef< IPropertyTableCell >& Cell, const TSharedRef< IPropertyTableUtilities >& Utilities, const FName& Style) const override;
	/** End IPropertyTableCustomColumn interface */

	/* The property type which can be displayed in this column */
	UProperty* EditProperty;
};