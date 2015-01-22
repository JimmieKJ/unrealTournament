// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotBrowser.cpp: Implements the SScreenShotBrowser class.
=============================================================================*/

#include "ScreenShotComparisonPrivatePCH.h"

/* SScreenShotBrowser interface
 *****************************************************************************/

void SScreenShotBrowser::Construct( const FArguments& InArgs,  IScreenShotManagerRef InScreenShotManager  )
{
	ScreenShotManager = InScreenShotManager;

	// Create the delegate for view change callbacks
	ScreenShotDelegate = FOnScreenFilterChanged::CreateSP( this, &SScreenShotBrowser::HandleScreenShotDataChanged );

	// Register for callbacks
	ScreenShotManager->RegisterScreenShotUpdate( ScreenShotDelegate );

	ChildSlot
	[
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			// Create the search bar.
			SNew( SScreenShotSearchBar, ScreenShotManager )
		]
		+SVerticalBox::Slot()
		.FillHeight( 1.0f )
		[
			SAssignNew( TreeBoxHolder, SHorizontalBox )
		]
	];

	ReGenerateTree();
}


TSharedRef<ITableRow> SScreenShotBrowser::OnGenerateWidgetForScreenView( TSharedPtr<IScreenShotData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	// Create the row widget.
	return
		SNew( SScreenViewRow, OwnerTable )
		. ScreenShotData( InItem );
}

void SScreenShotBrowser::HandleScreenShotDataChanged()
{
	// Need to do this as request refresh doesn't regenerate children
	ReGenerateTree();
}

void SScreenShotBrowser::ReGenerateTree()
{
	TreeBoxHolder->ClearChildren();

	TreeBoxHolder->AddSlot()
	.FillWidth( 1.0f )
	[
		SNew(SBorder)
		[
			SNew( SListView< TSharedPtr<IScreenShotData> > )
			.ListItemsSource( &ScreenShotManager->GetLists() )
			.OnGenerateRow( this, &SScreenShotBrowser::OnGenerateWidgetForScreenView )
			.SelectionMode( ESelectionMode::None )
		]
	];
}
