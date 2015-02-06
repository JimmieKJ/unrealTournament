// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenPlatformRow.h: Implements the SScreenPlatformRow class.
=============================================================================*/
#include "ScreenShotComparisonPrivatePCH.h"


void SScreenPlatformRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
{
 	ScreenShotDataItem = InArgs._ScreenShotDataItem;
	SMultiColumnTableRow< TSharedPtr< IScreenShotData > >::Construct( SMultiColumnTableRow< TSharedPtr< IScreenShotData > >::FArguments(), InOwnerTableView );
}


TSharedRef<SWidget> SScreenPlatformRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if ( ColumnName == TEXT( "Platform" ) )
	{
		// The platform
		return SNew( SBorder )
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Content()
		[
			SNew( STextBlock ) .Text( FText::FromString(ScreenShotDataItem->GetName()) )
		];
	}
	else if ( ColumnName == TEXT( "Current View" ) )
	{
		if ( ScreenShotDataItem->GetFilteredChildren().Num() > 0 )
		{
			return SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			[
				// Row with screen shot images
				SNew( SScreenShotItem ) .ScreenShotData( ScreenShotDataItem->GetFilteredChildren()[0] )
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				// Hack for spacer to align columns
				SNew( STextBlock ). Text( FText::FromString( TEXT(" ") ) )
			];
		}
	}
	else if ( ColumnName == TEXT( "History View" ) )
	{
		return	SNew( SScreenHistoryView ) .ScreenShotData( ScreenShotDataItem );
	}

	return SNew( STextBlock ). Text( NSLOCTEXT("UnrealFrontend", "PlatformCellContentError", "Error" ) );
}


