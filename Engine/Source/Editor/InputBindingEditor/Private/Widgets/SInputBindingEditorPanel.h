// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * A gesture sort functor.  Sorts by name or gesture and ascending or descending
 */
struct FChordSort
{
	FChordSort( bool bInSortName, bool bInSortUp )
		: bSortName( bInSortName )
		, bSortUp( bInSortUp )
	{ }

	bool operator()( const TSharedPtr<FChordTreeItem>& A, const TSharedPtr<FChordTreeItem>& B ) const
	{
		if( bSortName )
		{
			bool bResult = A->CommandInfo->GetLabel().CompareTo( B->CommandInfo->GetLabel() ) == -1;
			return bSortUp ? !bResult : bResult;
		}
		else
		{
			// Sort by binding
			bool bResult = A->CommandInfo->GetInputText().CompareTo( B->CommandInfo->GetInputText() ) == -1;
			return bSortUp ? !bResult : bResult;
		}
	}

private:

	/** Whether or not to sort by name.  If false we sort by binding. */
	bool bSortName;

	/** Whether or not to sort up.  If false we sort down. */
	bool bSortUp;
};


/**
 * The main input binding editor widget                   
 */
class SInputBindingEditorPanel : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SInputBindingEditorPanel){}
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	SInputBindingEditorPanel()
		: ChordSortMode( true, false )
	{ }

	/** Destructor. */
	virtual ~SInputBindingEditorPanel()
	{
		FInputBindingManager::Get().SaveInputBindings();
		FBindingContext::CommandsChanged.RemoveAll( this );
	}

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	void Construct(const FArguments& InArgs);

public:

	// SWidget interface

	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent ) override;

private:

	/**
	 * Called when the text changes in the search box.
	 * 
	 * @param NewSearch	The new search term to filter by.
	 */
	void OnSearchChanged( const FText& NewSearch );

	/**
	 * Generates widget for an item in the gesture tree.
	 */
	TSharedRef< ITableRow > OnGenerateWidgetForTreeItem( TSharedPtr<FChordTreeItem> InTreeItem, const TSharedRef<STableViewBase>& OwnerTable );

	/**
	 * Gets children FChordTreeItems from the passed in tree item.  Note: Only contexts have children and those children are the actual gestures.
	 */
	void OnGetChildrenForTreeItem( TSharedPtr<FChordTreeItem> InTreeItem, TArray< TSharedPtr< FChordTreeItem > >& OutChildren );

	/**
	 * Called when the binding column is clicked.  We sort by binding in this case .
	 */	 
	FReply OnBindingColumnClicked();

	/**
	 * Called when the name column is clicked. We sort by name in this case.
	 */	
	FReply OnNameColumnClicked();

	/** Updates the master context list with new commands. */
	void UpdateContextMasterList();

	/** Filters the currently visible context list. */
	void FilterVisibleContextList();

	/** Called when new commands are registered with the input binding manager. */
	void OnCommandsChanged();

private:

	/** List of all known contexts. */
	TArray< TSharedPtr<FChordTreeItem> > ContextMasterList;
	
	/** List of contexts visible in the tree. */
	TArray< TSharedPtr<FChordTreeItem> > ContextVisibleList;
	
	/** Search box */
	TSharedPtr<SWidget> SearchBox;

	/** Chord tree widget. */
	TSharedPtr< SChordTree > ChordTree;
	
	/** The current gesture sort to use. */
	FChordSort ChordSortMode;
	
	/** The current list of filter strings to filter gestures by. */
	TArray<FString> FilterStrings;
};
