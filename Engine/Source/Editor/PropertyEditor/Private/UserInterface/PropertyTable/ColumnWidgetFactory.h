// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once


class ColumnWidgetFactory : public TSharedFromThis< ColumnWidgetFactory >	
{
public:

	bool Supports( const TSharedRef < class IPropertyTableColumn >& Column );

	TSharedRef< class SColumnHeader > CreateColumnHeaderWidget( const TSharedRef < class IPropertyTableColumn >& Column, const TSharedRef< class IPropertyTableUtilities >& Utilities, const TSharedPtr< IPropertyTableCustomColumn >& Customization, const FName& Style );

};
