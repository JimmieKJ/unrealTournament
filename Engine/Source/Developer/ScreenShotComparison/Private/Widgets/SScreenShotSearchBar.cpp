// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotSearchBar.cpp: Implements the SScreenShotSearchBar class.
=============================================================================*/

#include "Widgets/SScreenShotSearchBar.h"
#include "Misc/FilterCollection.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"
#include "Models/ScreenShotComparisonFilter.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "ScreenShotBrowserToolBar"

void SScreenShotSearchBar::Construct( const FArguments& InArgs, IScreenShotManagerPtr InScreenShotManager )
{
	// Screen Shot manager to set the filter on
	ScreenShotManager = InScreenShotManager.ToSharedRef();

	// Create the search filter and set criteria
	ScreenShotComparisonFilter = MakeShareable( new FScreenShotComparisonFilter() ); 
	ScreenShotFilters = MakeShareable( new ScreenShotFilterCollection() );
	ScreenShotFilters->Add( ScreenShotComparisonFilter );

	ScreenShotManager->SetFilter( ScreenShotFilters );

	PlatformDisplayString = "Any";

	TArray< TSharedPtr<FString> >& PlatformList = ScreenShotManager->GetCachedPlatfomList();

	ChildSlot
	[
		SNew ( SHorizontalBox )
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 5.0f, 0.0f, 5.0f, 0.0f )
		[
			SNew(SSeparator)
			.Orientation(Orient_Vertical)
			.SeparatorImage( FEditorStyle::GetBrush(TEXT("Menu.Separator")) )
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("ScreenshotPlatform", "Platform:") )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SBox )
				.WidthOverride( 100.0f )
				[
					SNew(SComboButton)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text( this, &SScreenShotSearchBar::GetPlatformString )
					]
					.ContentPadding(FMargin(6.0f, 2.0f))
						.MenuContent()
						[
							SAssignNew(PlatformListView, SListView<TSharedPtr<FString> >)
							.ItemHeight(24.0f)
							.ListItemsSource(&PlatformList)
							.OnGenerateRow(this, &SScreenShotSearchBar::HandlePlatformListViewGenerateRow)
							.OnSelectionChanged(this, &SScreenShotSearchBar::HandlePlatformListSelected )
						]
				]
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 5.0f, 0.0f, 5.0f, 0.0f )
		[
			SNew(SSeparator)
			.Orientation(Orient_Vertical)
			.SeparatorImage( FEditorStyle::GetBrush(TEXT("Menu.Separator")) )
		]
		+ SHorizontalBox::Slot()
		.FillWidth( 0.3f )
		[
			SAssignNew( SearchBox, SSearchBox )
			.OnTextChanged( this, &SScreenShotSearchBar::HandleFilterTextChanged )
		]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonColorAndOpacity(FLinearColor(1.0f,1.0f,1.0f,0.0f))
				.OnClicked(this, &SScreenShotSearchBar::RefreshView)
				[
					SNew(SImage)
					.ToolTipText( LOCTEXT( "Refresh Screen shots", "Refresh Screen Shots" ) )
					.Image(FEditorStyle::GetBrush("AutomationWindow.RefreshTests.Small"))
				]
			]
	];
}

FText SScreenShotSearchBar::GetPlatformString() const
{
	return FText::FromString(PlatformDisplayString);
}

/** Filtering */

void SScreenShotSearchBar::HandleFilterTextChanged( const FText& InFilterText )
{
	FString TestName = *InFilterText.ToString();
	ScreenShotComparisonFilter->SetScreenFilter( TestName );
	ScreenShotManager->SetFilter( ScreenShotFilters );
}


void SScreenShotSearchBar::HandlePlatformListSelected( TSharedPtr<FString> PlatformName, ESelectInfo::Type SelectInfo )
{
	FString TestName;

	if( PlatformName.IsValid() )
	{
		TestName = *PlatformName;
		PlatformDisplayString = *PlatformName;
	}
	else
	{
		PlatformDisplayString = "Any";
	}

	if ( TestName == "Any" )
	{
		TestName = "";
	}

	ScreenShotComparisonFilter->SetPlatformFilter( TestName );
	ScreenShotManager->SetFilter( ScreenShotFilters );
}


TSharedRef<ITableRow> SScreenShotSearchBar::HandlePlatformListViewGenerateRow( TSharedPtr<FString> PlatformName, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
	.Content()
	[
		SNew( STextBlock ) .Text( FText::FromString(*PlatformName) )
	];
}

FReply SScreenShotSearchBar::RefreshView( )
{
	ScreenShotManager->GenerateLists();

	ScreenShotComparisonFilter->SetPlatformFilter( "" );
	ScreenShotManager->SetFilter( ScreenShotFilters );
	PlatformDisplayString = "Any";

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
