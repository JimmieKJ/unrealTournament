// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#define LOCTEXT_NAMESPACE "LevelsView"

typedef SListView< TSharedPtr< FLevelViewModel > > SLevelsListView;

/**
 * A slate widget that can be used to display a list of Levels and perform various Levels related actions
 */
class SLevelsView : public SCompoundWidget
{
public:
	typedef SListView< TSharedPtr< FLevelViewModel > >::FOnGenerateRow FOnGenerateRow;

public:

	SLATE_BEGIN_ARGS( SLevelsView ) {}
		SLATE_ATTRIBUTE( FText, HighlightText )
		SLATE_ARGUMENT( FOnContextMenuOpening, ConstructContextMenu )
		SLATE_EVENT( FOnGenerateRow, OnGenerateRow )
	SLATE_END_ARGS()

	/** SLevelsView constructor */
	SLevelsView()
	: bUpdatingSelection( false )
	{
		
	}

	/** SLevelsView destructor */
	~SLevelsView()
	{
		// Remove all delegates we registered
		ViewModel->OnLevelsChanged().RemoveAll( this );
		ViewModel->OnSelectionChanged().RemoveAll( this );
		ViewModel->OnDisplayActorCountChanged().RemoveAll( this );
		ViewModel->OnDisplayLightmassSizeChanged().RemoveAll( this );
		ViewModel->OnDisplayFileSizeChanged().RemoveAll( this ); 
		GLevelEditorModeTools().DeactivateMode( FBuiltinEditorModes::EM_Level );
	}

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InViewModel			The UI logic not specific to slate
	 */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs, const TSharedRef< FLevelCollectionViewModel >& InViewModel )
	{
		ViewModel = InViewModel;

		HighlightText = InArgs._HighlightText;
		FOnGenerateRow OnGenerateRowDelegate = InArgs._OnGenerateRow;

		if( !OnGenerateRowDelegate.IsBound() )
		{
			OnGenerateRowDelegate = FOnGenerateRow::CreateSP( this, &SLevelsView::OnGenerateRowDefault );
		}

		HeaderRowWidget =
			SNew( SHeaderRow )

			/** Level lock column */
			+ SHeaderRow::Column( LevelsView::ColumnID_Lock )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText( NSLOCTEXT("LevelsView", "Lock", "Lock") )
				]

			/** Level color column */
			+ SHeaderRow::Column( LevelsView::ColumnID_Color )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText( NSLOCTEXT("LevelsView", "Color", "Color") )
				]

			/** Level visibility column */
			+ SHeaderRow::Column( LevelsView::ColumnID_Visibility )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText( NSLOCTEXT("LevelsView", "Visibility", "Visibility") )
				]

			/** Level kismet column */
			+ SHeaderRow::Column( LevelsView::ColumnID_Kismet )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText( NSLOCTEXT("LevelsView", "Kismet", "Open the level blueprint for this Level") )
				]

			/** LevelName label column */
			+ SHeaderRow::Column( LevelsView::ColumnID_LevelLabel )
				.DefaultLabel( LOCTEXT("Column_LevelNameLabel", "Level") )
				.FillWidth( 0.45f )

			/** Level SCC status column */
			+ SHeaderRow::Column( LevelsView::ColumnID_SCCStatus )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText( NSLOCTEXT("LevelsView", "SCCStatus", "Status in Source Control") )
				]

			/** Level save column */
			+ SHeaderRow::Column( LevelsView::ColumnID_Save )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText( NSLOCTEXT("LevelsView", "Save", "Save this Level") )
				];

		ChildSlot
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.FillHeight( 1.0f )
				[
					SAssignNew( ListView, SLevelsListView )

					// Enable multi-select if we're in browsing mode, single-select if we're in picking mode
					.SelectionMode( ESelectionMode::Multi )

					// Point the tree to our array of root-level items.  Whenever this changes, we'll call RequestTreeRefresh()
					.ListItemsSource( &ViewModel->GetLevels() )

					// Find out when the user selects something in the tree
					.OnSelectionChanged( this, &SLevelsView::OnSelectionChanged )

					// Called when the user double-clicks with LMB on an item in the list
					.OnMouseButtonDoubleClick( this, &SLevelsView::OnListViewMouseButtonDoubleClick )

					// Generates the actual widget for a tree item
					.OnGenerateRow( OnGenerateRowDelegate ) 

					// Use the level viewport context menu as the right click menu for list items
					.OnContextMenuOpening( InArgs._ConstructContextMenu )

					// Header for the tree
					.HeaderRow( HeaderRowWidget.ToSharedRef() )
				]
			];

		// This needs to be set to the column before the "Save" column.
		InsertColumnIndex = 5;

		if(GetDefault<ULevelBrowserSettings>()->bDisplayActorCount)
		{
			ToggleActorCount(true);
		}

		if(GetDefault<ULevelBrowserSettings>()->bDisplayLightmassSize)
		{
			ToggleLightmassSize(true);
		}

		if(GetDefault<ULevelBrowserSettings>()->bDisplayFileSize)
		{
			ToggleFileSize(true);
		}

		if(GetDefault<ULevelBrowserSettings>()->bDisplayEditorOffset)
		{
			ShowEditorOffsetColumn(true);
		}

		ViewModel->OnLevelsChanged().AddSP( this, &SLevelsView::RequestRefresh );
		ViewModel->OnSelectionChanged().AddSP( this, &SLevelsView::UpdateSelection );
		ViewModel->OnDisplayActorCountChanged().AddSP( this, &SLevelsView::ToggleActorCount );
		ViewModel->OnDisplayLightmassSizeChanged().AddSP( this, &SLevelsView::ToggleLightmassSize );
		ViewModel->OnDisplayFileSizeChanged().AddSP( this, &SLevelsView::ToggleFileSize );
		ViewModel->OnDisplayEditorOffsetChanged().AddSP( this, &SLevelsView::ShowEditorOffsetColumn );
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION


protected:

	/**
	 * Checks to see if this widget supports keyboard focus.  Override this in derived classes.
	 *
	 * @return  True if this widget can take keyboard focus
	 */
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/**
	 * Called after a key is pressed when this widget has focus
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InKeyEvent  Key event
	 *
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override
	{
		return ViewModel->GetCommandList()->ProcessCommandBindings( InKeyEvent ) ? FReply::Handled() : FReply::Unhandled();
	}

	/**
	 * Called during drag and drop when the drag leaves a widget.
	 *
	 * @param DragDropEvent   The drag and drop event.
	 */
	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override
	{
		TSharedPtr< FActorDragDropGraphEdOp > DragActorOp = DragDropEvent.GetOperationAs< FActorDragDropGraphEdOp >();
		if (DragActorOp.IsValid())
		{
			DragActorOp->ResetToDefaultToolTip();
		}
	}

	//@todo: add actor drag + drop support
	/**
	 * Called during drag and drop when the the mouse is being dragged over a widget.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override
	{

		TSharedPtr< FActorDragDropGraphEdOp > DragActorOp = DragDropEvent.GetOperationAs< FActorDragDropGraphEdOp >();
		if (!DragActorOp.IsValid())
		{
			return FReply::Unhandled();
		}

		DragActorOp->SetToolTip( FActorDragDropGraphEdOp::ToolTip_CompatibleGeneric, LOCTEXT("OnDragOver", "Add Actors to New Level") );

		// We leave the event unhandled so the children of the ListView get a chance to grab the drag/drop
		return FReply::Unhandled();
	}

	//@todo: add actor drag + drop support
	/**
	 * Called when the user is dropping something onto a widget; terminates drag and drop.
	 *
	 * @param MyGeometry      The geometry of the widget receiving the event.
	 * @param DragDropEvent   The drag and drop event.
	 *
	 * @return A reply that indicated whether this event was handled.
	 */
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override
	{
		const bool bIsValidDrop = DragDropEvent.GetOperationAs<FActorDragDropGraphEdOp>().IsValid();
		return bIsValidDrop ? FReply::Handled() : FReply::Unhandled();
	}


private:

	/** 
	 *	Called by SListView to generate a table row for the specified item.
	 *
	 *	@param	Item		The FLevelViewModel to generate a TableRow widget for
	 *	@param	OwnerTable	The TableViewBase which will own the generated TableRow widget
	 *
	 *	@return The generated TableRow widget
	 */
	TSharedRef< ITableRow > OnGenerateRowDefault( const TSharedPtr< FLevelViewModel > Item, const TSharedRef< STableViewBase >& OwnerTable )
	{
		return SNew( SLevelsViewRow, Item.ToSharedRef(), OwnerTable )
			.HighlightText( HighlightText );
	}

	/**
	 *	Kicks off a Refresh of the LevelsView
	 *
	 *	@param	Action				The action taken on one or more Levels
	 *	@param	ChangedLevel		The Level that changed
	 *	@param	ChangedProperty		The property that changed
	 */
	void RequestRefresh( const ELevelsAction::Type Action, const TWeakObjectPtr< ULevel >& ChangedLevel, const FName& ChangedProperty )
	{
		ListView->RequestListRefresh();
	}

	/**
	 *	Called whenever the viewmodel's selection changes
	 */
	void UpdateSelection()
	{
		if( bUpdatingSelection )
		{
			return;
		}

		bUpdatingSelection = true;
		const auto& SelectedLevels = ViewModel->GetSelectedLevels();
		ListView->ClearSelection();
		for( auto LevelIter = SelectedLevels.CreateConstIterator(); LevelIter; ++LevelIter )
		{
			ListView->SetItemSelection( *LevelIter, true );
		}

		if( SelectedLevels.Num() == 1 )
		{
			ListView->RequestScrollIntoView( SelectedLevels[ 0 ] );
		}
		bUpdatingSelection = false;
	}

	/** 
	 *	Called by SListView when the selection has changed 
	 *
	 *	@param	Item	The Level affected by the selection change
	 */
	void OnSelectionChanged( const TSharedPtr< FLevelViewModel > Item, ESelectInfo::Type SelectInfo )
	{
		if( bUpdatingSelection )
		{
			return;
		}

		bUpdatingSelection = true;
		ViewModel->SetSelectedLevels( ListView->GetSelectedItems() );
		bUpdatingSelection = false;
	}

	/** 
	 *	Called by SListView when the user double-clicks on an item 
	 *
	 *	@param	Item	The Level that was double clicked
	 */
	void OnListViewMouseButtonDoubleClick( const TSharedPtr< FLevelViewModel > Item )
	{
		Item->MakeLevelCurrent();
	}

	/** Callback for when toggling to view Actor Count. */
	void ToggleActorCount(bool bInEnabled)
	{
		if(bInEnabled)
		{
			HeaderRowWidget->InsertColumn( 
				SHeaderRow::Column( LevelsView::ColumnID_ActorCount )
				.FillWidth( 0.15f )
				.HeaderContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("Column_ActorCountLabel", "Actors") )
						.ToolTipText( LOCTEXT("Column_ActorCountLabel", "Actors") )
					]
				], InsertColumnIndex);
			++InsertColumnIndex;
		}
		else
		{
			HeaderRowWidget->RemoveColumn(LevelsView::ColumnID_ActorCount);
			--InsertColumnIndex;
		}
	}

	/** Callback for when toggling to view Lightmass Size. */
	void ToggleLightmassSize(bool bInEnabled)
	{
		if(bInEnabled)
		{
			HeaderRowWidget->InsertColumn( 
				SHeaderRow::Column( LevelsView::ColumnID_LightmassSize )
				.FillWidth( 0.4f )
				.HeaderContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("Column_LightmassSizeLabel", "Lightmass Size (MB)") )
						.ToolTipText( LOCTEXT("Column_LightmassSizeLabel", "Lightmass Size (MB)") )
					]
				], InsertColumnIndex);
			++InsertColumnIndex;
		}
		else
		{
			HeaderRowWidget->RemoveColumn(LevelsView::ColumnID_LightmassSize);
			--InsertColumnIndex;
		}
	}

	/** Callback for when toggling to view File Size. */
	void ToggleFileSize(bool bInEnabled)
	{
		if(bInEnabled)
		{
			HeaderRowWidget->InsertColumn( 
				SHeaderRow::Column( LevelsView::ColumnID_FileSize )
				.FillWidth( 0.4f )
				.HeaderContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("Column_FileSizeLabel", "File Size (MB)") )
						.ToolTipText( LOCTEXT("Column_FileSizeLabel", "File Size (MB)") )
					]
				], InsertColumnIndex);
			++InsertColumnIndex;
		}
		else
		{
			HeaderRowWidget->RemoveColumn(LevelsView::ColumnID_FileSize);
			--InsertColumnIndex;
		}
	}

	/** Callback for when toggling to view the Editor Offset column. */
	void ShowEditorOffsetColumn(bool bInShow)
	{
		if(bInShow)
		{
			HeaderRowWidget->InsertColumn( 
				SHeaderRow::Column( LevelsView::ColumnID_EditorOffset )
				.FillWidth( 0.4f )
				.HeaderContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text( LOCTEXT("Column_EditorOffsetLabel", "Editor Offset") )
					]
				], InsertColumnIndex);
			++InsertColumnIndex;
		}
		else
		{
			HeaderRowWidget->RemoveColumn(LevelsView::ColumnID_EditorOffset);
			--InsertColumnIndex;
		}
	}


private:

	/**	Whether the view is currently updating the viewmodel selection */
	bool bUpdatingSelection;

	/** The UI logic of the LevelsView that is not Slate specific */
	TSharedPtr< FLevelCollectionViewModel > ViewModel;

	/** Our list view used in the SLevelsViews */
	TSharedPtr< SLevelsListView > ListView;

	/** The string to highlight on any text contained in the view widget */
	TAttribute< FText > HighlightText;

	/** The Header Row for the list view */
	TSharedPtr< SHeaderRow > HeaderRowWidget;

	/** The index to insert columns at. */
	int32 InsertColumnIndex;
};


#undef LOCTEXT_NAMESPACE
