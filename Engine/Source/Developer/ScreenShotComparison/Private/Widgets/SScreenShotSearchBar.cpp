// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SScreenShotSearchBar.cpp: Implements the SScreenShotSearchBar class.
=============================================================================*/

#include "ScreenShotComparisonPrivatePCH.h"
#include "SSearchBox.h"

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

	//Default to show all screenshots
	DisplayEveryNthScreenshot = 1;

	TArray< TSharedPtr<FString> >& PlatformList = ScreenShotManager->GetCachedPlatfomList();

	ChildSlot
	[
		SNew ( SHorizontalBox )
		+ SHorizontalBox::Slot()
		.FillWidth( 1.0f )
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
				.Text( LOCTEXT("ScreenshotNthLabel", "Display Every Nth Screenshot:") )
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew( SBox )
				.Padding( FMargin(4.0f, 0.0f, 0.0f, 0.0f) )
				.WidthOverride( 100.0f )
				[
					SNew(SSpinBox<int32>)
					.MinValue(1)
					.MaxValue(20)
					.MinSliderValue(1)
					.MaxSliderValue(20)
					.Value(this,&SScreenShotSearchBar::GetDisplayEveryNthScreenshot)
					.OnValueChanged(this,&SScreenShotSearchBar::OnChangedDisplayEveryNthScreenshot)
					.OnValueCommitted(this,&SScreenShotSearchBar::OnCommitedDisplayEveryNthScreenshot)
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

void SScreenShotSearchBar::OnCommitedDisplayEveryNthScreenshot(int32 InNewValue, ETextCommit::Type CommitType)
{
	DisplayEveryNthScreenshot = InNewValue;
	ScreenShotManager->SetDisplayEveryNthScreenshot(DisplayEveryNthScreenshot);
}

void SScreenShotSearchBar::OnChangedDisplayEveryNthScreenshot(int32 InNewValue)
{
	DisplayEveryNthScreenshot = InNewValue;
}

int32 SScreenShotSearchBar::GetDisplayEveryNthScreenshot() const
{
	return DisplayEveryNthScreenshot;
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
	DisplayEveryNthScreenshot = 1;

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
