// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "SToolkitDisplay.h"
#include "SLevelEditorToolBox.h"
#include "Toolkits/IToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/SAssetEditorCommon.h"
#include "SHyperlink.h"

#define LOCTEXT_NAMESPACE "ToolkitDisplay"



void SLevelEditorActiveToolkit::Construct( const FArguments&, const TSharedPtr< IToolkit >& InitToolkit, const FEdMode* InitEditorMode )
{
	Toolkit = InitToolkit;
	EditorMode = InitEditorMode;

	ActiveToolkitType = Toolkit.IsValid() ? ELevelEditorActiveToolkit::Toolkit : ELevelEditorActiveToolkit::LegacyEditorMode;
	check( ( ActiveToolkitType == ELevelEditorActiveToolkit::Toolkit && Toolkit.IsValid() ) ||
		   ( ActiveToolkitType == ELevelEditorActiveToolkit::LegacyEditorMode && EditorMode != NULL ) );

	const bool bIsAssetEditorToolkit = ActiveToolkitType == ELevelEditorActiveToolkit::Toolkit && InitToolkit->IsAssetEditor();

	// @todo toolkit major: Improve look of this:
	//		- draw icon for asset editor (e.g. blueprint icon)
	//				- Probably should appear in standalone version too!
	//		- make text more pronounced!  maybe use two lines (put editor name on second line, smaller text?)
	//		- combo drop-down looks horrible...
	//
	//		- highlight all related tabs on mouse over
	//		- maybe change to task bar styling?
	//		- icon to "tear out and edit standalone"?  Or drag tear?
	//		- what about asset type or path?
	//		- animation transitions when mode is opened and killed?
	//		- animation when focusing tabs?
	//
	//		- new toolkit doc area tabs should animate in (like they do when closing!) (see CreateNewTabStackBySplitting)
	//		- need visual cue when there are no toolkits around (currently just empty expando)
	//		- color distinction between modes and asset editors?

	TSharedPtr< SHorizontalBox > ContentBox;

	ChildSlot
		[
			SNew( SBorder )
				.Padding( 3.0f )
				[
					SNew( SOverlay )
						+SOverlay::Slot()
						[
							SNew(SImage)
								// Don't allow color overlay to absorb mouse clicks
								.Visibility( EVisibility::HitTestInvisible )
								.Image( FEditorStyle::GetBrush( "ToolkitDisplay.ColorOverlay" ) )
								.ColorAndOpacity( this, &SLevelEditorActiveToolkit::GetToolkitBackgroundOverlayColor )
						]
	
						+SOverlay::Slot()
						[
							SAssignNew( ContentBox, SHorizontalBox )
								+SHorizontalBox::Slot()
									.FillWidth( 1.0f )
									[
										SNew( SHorizontalBox )
											+SHorizontalBox::Slot()
												.AutoWidth()
												.Padding( 4.0f, 7.0f, 3.0f, 3.0f )
												[
													// @todo toolkit major: Separate asset name from editor name (and draw unsaved change state next to asset part)
													SNew( SHyperlink )
														.Text( this, &SLevelEditorActiveToolkit::GetToolkitTextLabel )
														.OnNavigate( this, &SLevelEditorActiveToolkit::OnNavigateToToolkit )
												]

											// Asset modified state
											+SHorizontalBox::Slot()
											.AutoWidth()
											.Padding( 0.0f, 0.0f, 3.0f, 0.0f )
											[
												SNew( SImage )
												.Visibility( this, &SLevelEditorActiveToolkit::GetVisibilityForUnsavedChangeIcon )
												.Image( FEditorStyle::GetBrush( "ToolkitDisplay.UnsavedChangeIcon" ) )
												.ToolTipText( LOCTEXT("UnsavedChangeToolTip", "This asset has unsaved changes") )
											]
									]
						]
				]
		];


	if( bIsAssetEditorToolkit )
	{
		TSharedRef< FAssetEditorToolkit > AssetEditorToolkit = StaticCastSharedPtr< FAssetEditorToolkit >( Toolkit ).ToSharedRef();
		// We want the window to be closed after the user chooses an option from the drop-down
		const bool bIsPopup = true;

		ContentBox->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign( HAlign_Right )
			[
				SNew( SComboButton )
					.ComboButtonStyle(FEditorStyle::Get(), "ToolkitDisplay.ComboButton")
					.ButtonContent()
					[
						SNew(SImage)
							.Image( FEditorStyle::GetBrush( "ToolkitDisplay.MenuDropdown" ) )
					]
					.MenuContent()
					[
						SNew( SAssetEditorCommon, AssetEditorToolkit, bIsPopup )
					]
			];
	}


	ContentBox->AddSlot()
		.AutoWidth()
		.HAlign( HAlign_Right )	// @todo toolkit major: Needs hover cue, visual polish, margins (probably should be consistent with tab close button)
		.Padding( 2.0f )
		[
			SNew( SButton )
				.OnClicked( this, &SLevelEditorActiveToolkit::OnToolkitCloseButtonClicked )
				.ToolTipText( LOCTEXT("CloseToolkitButton", "Close") )
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.ContentPadding(0)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ForegroundColor( FSlateColor::UseForeground() )
				[
					SNew(SImage)
						.Image( FEditorStyle::GetBrush( "Symbols.X" ) )
						.ColorAndOpacity( FSlateColor::UseForeground() )
				]
		];
}


FText SLevelEditorActiveToolkit::GetToolkitTextLabel() const
{
	if( ActiveToolkitType == ELevelEditorActiveToolkit::LegacyEditorMode )
	{
		return EditorMode->GetModeInfo().Name;
	}
	else if( ActiveToolkitType == ELevelEditorActiveToolkit::Toolkit )
	{
		return Toolkit->GetToolkitName();
	}
	return LOCTEXT("UnknownToolkitName", "???" );
}


FSlateColor SLevelEditorActiveToolkit::GetToolkitBackgroundOverlayColor() const
{
	if( ActiveToolkitType == ELevelEditorActiveToolkit::LegacyEditorMode )
	{
		// Don't bother coloring legacy editor modes
		return FLinearColor( 0, 0, 0, 0 );
	}
	else if( ActiveToolkitType == ELevelEditorActiveToolkit::Toolkit )
	{
		return Toolkit->GetWorldCentricTabColorScale();
	}
	return FLinearColor( 0, 0, 0, 0 );
}

void SLevelEditorActiveToolkit::DeactivateMode()
{
	if( ActiveToolkitType == ELevelEditorActiveToolkit::LegacyEditorMode )
	{
		// Exit the mode.  This will immediately call back to remove the active toolkit from our list
		check( EditorMode != NULL );
		GLevelEditorModeTools().DeactivateMode( EditorMode->GetID() );
	}
	else if( ActiveToolkitType == ELevelEditorActiveToolkit::Toolkit )
	{
		// @todo toolkit minor: Should we force a "Save and Close" workflow?  Basically, expect assets to be saved before they are closed out (and could be GC'd?)
		//			-> Doesn't really "work" unless we store off working copies (like material editor)

		// NOTE: This will call back to the toolkit area and actually remove this active toolkit from our list!
		if( Toolkit.IsValid() == true )
		{
			FToolkitManager::Get().CloseToolkit( Toolkit.ToSharedRef() );
		}
	}
}


FReply SLevelEditorActiveToolkit::OnToolkitCloseButtonClicked()
{
	DeactivateMode();
	return FReply::Handled();
}


EVisibility SLevelEditorActiveToolkit::GetVisibilityForUnsavedChangeIcon() const
{
	bool bShowUnsavedChangeIcon = false;
	const bool bIsAssetEditorToolkit = ActiveToolkitType == ELevelEditorActiveToolkit::Toolkit && Toolkit->IsAssetEditor();
	if( bIsAssetEditorToolkit )
	{
		const TSharedRef< FAssetEditorToolkit >& AssetEditorToolkit = StaticCastSharedRef< FAssetEditorToolkit >( Toolkit.ToSharedRef() );
		const TArray< UObject* >& Assets = *AssetEditorToolkit->GetObjectsCurrentlyBeingEdited();

		for( int AssetIndex = 0; !bShowUnsavedChangeIcon && AssetIndex < Assets.Num(); ++AssetIndex )
		{
			const auto Asset = Assets[ AssetIndex ];
			if( Asset != NULL )
			{
				bShowUnsavedChangeIcon = Asset->GetOutermost()->IsDirty();
			}
		}
	}

	return bShowUnsavedChangeIcon ? EVisibility::Visible : EVisibility::Collapsed;
}


void SLevelEditorActiveToolkit::OnNavigateToToolkit( )
{
	if( ActiveToolkitType == ELevelEditorActiveToolkit::Toolkit )
	{
		// User clicked the tool tip text, so we'll bring all of this toolkit's tabs to the front!
		Toolkit->BringToolkitToFront();
	}
}


void SToolkitDisplay::Construct( const FArguments& InArgs, const TSharedRef< class ILevelEditor >& OwningLevelEditor )
{
	OnInlineContentChangedDelegate = InArgs._OnInlineContentChanged;

	ChildSlot
		[
			SAssignNew( VBox, SVerticalBox )
		];

	// Register with the mode system to find out when a mode is entered or exited
	GLevelEditorModeTools().OnEditorModeChanged().AddSP( SharedThis( this ), &SToolkitDisplay::OnEditorModeChanged );

	// Find all of the current active modes and toolkits and add those right away.  This widget could have been created "late"!
	{
		TArray< FEdMode* > ActiveModes;
		GLevelEditorModeTools().GetActiveModes( ActiveModes );
		for( auto EdModeIt = ActiveModes.CreateConstIterator(); EdModeIt; ++EdModeIt )
		{
			// We don't care about the default editor mode.  Just ignore it.
			if( (*EdModeIt)->GetID() != FBuiltinEditorModes::EM_Default && !(*EdModeIt)->UsesToolkits() )
			{
				AddEditorMode( *EdModeIt );
			}
		}

		const TArray< TSharedPtr< IToolkit > >& HostedToolkits = OwningLevelEditor->GetHostedToolkits();
		for( auto HostedToolkitIt = HostedToolkits.CreateConstIterator(); HostedToolkitIt; ++HostedToolkitIt )
		{
			OnToolkitHostingStarted( ( *HostedToolkitIt ).ToSharedRef() );
		}
	}
}


SToolkitDisplay::~SToolkitDisplay()
{
	//the toolkit is bieng destroyed - deactivate any modes that were active for this toolkit
	for( auto ToolkitIt = ActiveToolkits.CreateConstIterator(); ToolkitIt; ++ToolkitIt )
	{
		( *ToolkitIt )->DeactivateMode();		
	}

	// Unregister delegates
	GLevelEditorModeTools().OnEditorModeChanged().RemoveAll( this );
}


void SToolkitDisplay::AddEditorMode( FEdMode* EditorMode )
{
	// @todo toolkit major: should we move these somewhere where they won't get covered up by world-centric! (tool bar? perma-tab? viewport overlay?)

	TSharedRef< SLevelEditorActiveToolkit > NewActiveToolkit =
		SNew( SLevelEditorActiveToolkit, TSharedPtr<IToolkit>(), EditorMode );

	ActiveToolkits.Add( NewActiveToolkit );

	VBox->AddSlot()	
		.AutoHeight()
		[
			NewActiveToolkit
		];
}


void SToolkitDisplay::RemoveEditorMode( FEdMode* EditorMode )
{
	for( auto ToolkitIt = ActiveToolkits.CreateConstIterator(); ToolkitIt; ++ToolkitIt )
	{
		if( ( *ToolkitIt )->EditorMode == EditorMode )
		{
			// Remove this widget!
			VBox->RemoveSlot( ToolkitIt->ToSharedRef() );
			ActiveToolkits.RemoveAt( ToolkitIt.GetIndex() );
			break;
		}
	}
}


void SToolkitDisplay::OnEditorModeChanged( FEdMode* EditorMode, bool bEntered )
{
	check( EditorMode != NULL );

	// @todo toolkit minor: Really, we should only be listening about editor modes that are related to the SLevelEditor's world!

	// We don't care about the default editor mode.  Just ignore it.
	if( EditorMode->GetID() != FBuiltinEditorModes::EM_Default && !EditorMode->UsesToolkits())
	{
		if( bEntered )
		{
			AddEditorMode( EditorMode );
		}
		else
		{
			RemoveEditorMode( EditorMode );
		}
	}
}


void SToolkitDisplay::AddToolkit( const TSharedRef< IToolkit >& NewToolkit )
{
	TSharedRef< SLevelEditorActiveToolkit > NewActiveToolkit =
		SNew( SLevelEditorActiveToolkit, NewToolkit, (FEdMode*)NULL );
	
	ActiveToolkits.Add( NewActiveToolkit );

	VBox->AddSlot()	
		.AutoHeight()
		[
			NewActiveToolkit
		];
}


void SToolkitDisplay::RemoveToolkit( const TSharedRef< IToolkit >& DestroyingToolkit )
{
	for( auto ToolkitIt = ActiveToolkits.CreateConstIterator(); ToolkitIt; ++ToolkitIt )
	{
		if( ( *ToolkitIt )->Toolkit == DestroyingToolkit )
		{
			// Remove this widget!
			VBox->RemoveSlot( ToolkitIt->ToSharedRef() );
			ActiveToolkits.RemoveAt( ToolkitIt.GetIndex() );
			break;
		}
	}
}


void SToolkitDisplay::OnToolkitHostingStarted( const TSharedRef< IToolkit >& NewToolkit )
{
	if (NewToolkit->GetEditorMode())
	{
		AddEditorMode( NewToolkit->GetEditorMode() );
		OnInlineContentChangedDelegate.ExecuteIfBound(NewToolkit->GetInlineContent().ToSharedRef());
	}
	else
	{
		AddToolkit( NewToolkit );
	}
}


void SToolkitDisplay::OnToolkitHostingFinished( const TSharedRef< IToolkit >& DestroyingToolkit )
{
	if (DestroyingToolkit->GetEditorMode())
	{
		RemoveEditorMode( DestroyingToolkit->GetEditorMode() );
		OnInlineContentChangedDelegate.ExecuteIfBound(SNullWidget::NullWidget);
	}
	else
	{
		RemoveToolkit( DestroyingToolkit );
	}
}


#undef LOCTEXT_NAMESPACE
