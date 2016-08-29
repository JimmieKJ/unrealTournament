// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TaskGraphPrivatePCH.h"
#include "SlateBasics.h"
#include "TaskGraphStyle.h"
#include "SProfileVisualizer.h"
#include "TaskGraphInterfaces.h"
#include "STaskGraph.h"
#include "SGraphBar.h"
#include "SBarVisualizer.h"
#include "SEventsTree.h"
	
void SProfileVisualizer::Construct( const FArguments& InArgs )
{
	ProfileData = InArgs._ProfileData;
	ProfilerType = InArgs._ProfilerType;
	HeaderMessageText = InArgs._HeaderMessageText;
	HeaderMessageTextColor = InArgs._HeaderMessageTextColor;

	const FSlateBrush* ContentAreaBrush = FTaskGraphStyle::Get()->GetBrush("TaskGraph.ContentAreaBrush");
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			.BorderImage(ContentAreaBrush)
			[
				SNew(STextBlock)
				.Visibility( HeaderMessageText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible )
				.Text(HeaderMessageText)
				.ColorAndOpacity(HeaderMessageTextColor)
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot()
		[
			SAssignNew( MainSplitter, SSplitter )
			.Orientation( Orient_Vertical )
			+ SSplitter::Slot()
			.Value(1.0f)
			[
				SNew( SBorder )
				.Visibility( EVisibility::Visible )
				.BorderImage( ContentAreaBrush )
				[
					SAssignNew( BarVisualizer, SBarVisualizer )
					.ProfileData( ProfileData )
					.OnBarGraphSelectionChanged( this, &SProfileVisualizer::RouteBarGraphSelectionChanged )
					.OnBarGraphExpansionChanged( this, &SProfileVisualizer::RouteBarGraphExpansionChanged )
					.OnBarEventSelectionChanged( this, &SProfileVisualizer::RouteBarEventSelectionChanged )
					.OnBarGraphContextMenu( this, &SProfileVisualizer::OnBarGraphContextMenu )
				]
			]
			+ SSplitter::Slot()
			.Value(1.0f)
			[
				SNew( SBorder )
				.Visibility( EVisibility::Visible )
				.BorderImage( ContentAreaBrush )
				[
					SAssignNew( EventsTree, SEventsTree )
					.ProfileData( ProfileData )
					.OnEventSelectionChanged( this, &SProfileVisualizer::RouteEventSelectionChanged )
				]
			]
		]
	];

	// Attempt to choose initial data set to display in the tree view
	EventsTree->HandleBarGraphExpansionChanged( ProfileData );
}

TSharedRef< SWidget > SProfileVisualizer::MakeMainMenu()
{
	FMenuBarBuilder MenuBuilder( NULL );
	{
		// File
		MenuBuilder.AddPullDownMenu( 
			NSLOCTEXT("TaskGraph", "FileMenu", "File"),
			NSLOCTEXT("TaskGraph", "FileMenu_ToolTip", "Open the file menu"),
			FNewMenuDelegate::CreateSP( this, &SProfileVisualizer::FillFileMenu ) );

		// Apps
		MenuBuilder.AddPullDownMenu( 
			NSLOCTEXT("TaskGraph", "AppMenu", "Window"),
			NSLOCTEXT("TaskGraph", "AppMenu_ToolTip", "Open the summoning menu"),
			FNewMenuDelegate::CreateSP( this, &SProfileVisualizer::FillAppMenu ) );

		// Help
		MenuBuilder.AddPullDownMenu( 
			NSLOCTEXT("TaskGraph", "HelpMenu", "Help"),
			NSLOCTEXT("TaskGraph", "HelpMenu_ToolTip", "Open the help menu"),
			FNewMenuDelegate::CreateSP( this, &SProfileVisualizer::FillHelpMenu ) );
	}

	// Create the menu bar
	TSharedRef< SWidget > MenuBarWidget = MenuBuilder.MakeWidget();

	return MenuBarWidget;
}

void SProfileVisualizer::FillFileMenu( FMenuBuilder& MenuBuilder )
{
}

void SProfileVisualizer::FillAppMenu( FMenuBuilder& MenuBuilder )
{
}

void SProfileVisualizer::FillHelpMenu( FMenuBuilder& MenuBuilder )
{
}


void SProfileVisualizer::RouteEventSelectionChanged( TSharedPtr< FVisualizerEvent > Selection )
{
	BarVisualizer->HandleEventSelectionChanged( Selection );
}

void SProfileVisualizer::RouteBarGraphSelectionChanged( TSharedPtr< FVisualizerEvent > Selection )
{
	EventsTree->HandleBarGraphSelectionChanged( Selection );
}

void SProfileVisualizer::RouteBarGraphExpansionChanged( TSharedPtr< FVisualizerEvent > Selection )
{
	EventsTree->HandleBarGraphExpansionChanged( Selection );
}

void SProfileVisualizer::RouteBarEventSelectionChanged( int32 Thread, TSharedPtr<FVisualizerEvent> Selection )
{
	EventsTree->HandleBarEventSelectionChanged( Thread, Selection );
}

void SProfileVisualizer::OnBarGraphContextMenu( TSharedPtr< FVisualizerEvent > Selection, const FPointerEvent& InputEvent )
{
	SelectedBarGraph = Selection;

	FWidgetPath WidgetPath = InputEvent.GetEventPath() != nullptr ? *InputEvent.GetEventPath() : FWidgetPath();
	FSlateApplication::Get().PushMenu(SharedThis(this), WidgetPath, MakeBarVisualizerContextMenu(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect::ContextMenu);
}

TSharedRef<SWidget> SProfileVisualizer::MakeBarVisualizerContextMenu()
{
	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder( bCloseAfterSelection, NULL );
	{
		FUIAction Action( FExecuteAction::CreateSP( this, &SProfileVisualizer::ShowGraphBarInEventsWindow, (int32)INDEX_NONE ) );
		MenuBuilder.AddMenuEntry( NSLOCTEXT("TaskGraph", "GraphBarShowInNew", "Show in New Events Window"), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::Button );
	}
		
	return MenuBuilder.MakeWidget();
}

void SProfileVisualizer::ShowGraphBarInEventsWindow( int32 WindowIndex )
{
	EventsTree->HandleBarGraphExpansionChanged( SelectedBarGraph );
}

