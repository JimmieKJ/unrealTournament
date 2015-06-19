// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR
#include "AssetData.h"
#endif

/* 
 * Super search - Searches a number of resources including documentation, tutorials, Wiki and AnswerHub
 */

class SSuperSearchBox
	: public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SSuperSearchBox )
		: _Style()
		, _SuggestionListPlacement( MenuPlacement_ComboBoxRight )
		{}

		/** Style used to draw this search box */
		SLATE_ARGUMENT( TOptional<const FSearchBoxStyle*>, Style )

		/** Where to place the suggestion list */
		SLATE_ARGUMENT( EMenuPlacement, SuggestionListPlacement )

	SLATE_END_ARGS()

	/** Protected console input box widget constructor, called by Slate */
	SSuperSearchBox();

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs	Declaration used by the SNew() macro to construct this widget
	 */
	void Construct( const FArguments& InArgs );

	/** Returns the editable text box associated with this widget.  Used to set focus directly. */
	TSharedRef< SEditableTextBox > GetEditableTextBox()
	{
		return InputText.ToSharedRef();
	}
		
protected:

	virtual bool SupportsKeyboardFocus() const { return true; }

	// e.g. Tab or Key_Up
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent );

	void OnFocusLost( const FFocusEvent& InFocusEvent );

	/** Handles entering in a command */
	void OnTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);

	void OnTextChanged(const FText& InText);

	/** Makes the widget for the suggestions messages in the list view */
	TSharedRef<ITableRow> MakeSuggestionListItemWidget(TSharedPtr<FSearchEntry> Suggestion, const TSharedRef<STableViewBase>& OwnerTable);

	void Query_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	void SuggestionSelectionChanged(TSharedPtr<FSearchEntry> NewValue);
	
	void OnMenuOpenChanged(bool bIsOpen);

	void ActOnSuggestion(TSharedPtr<FSearchEntry> SearchEntry, FString const& Category);
		
	void UpdateSuggestions();

	void MarkActiveSuggestion();

	void ClearSuggestions();

	FString GetSelectionText() const;

private:

	/** Editable text widget */
	TSharedPtr< SEditableTextBox > InputText;

	/** history / auto completion elements */
	TSharedPtr< SMenuAnchor > SuggestionBox;

	/** All suggestions stored in this widget for the list view */
	TArray< TSharedPtr<FSearchEntry> > Suggestions;

	/** The list view for showing all log messages. Should be replaced by a full text editor */
	TSharedPtr< SListView< TSharedPtr<FSearchEntry> > > SuggestionListView;

	//TODO: properly clean map up as time goes by requests are processed and can be discarded
	TMap<FHttpRequestPtr, FText> RequestQueryMap;

	typedef TMap<FString, TArray<FSearchEntry> > FCategoryResults;
	struct FSearchResults
	{
		 FCategoryResults OnlineResults;
		 TArray<FSearchEntry> TutorialResults;
	};

	//TODO: properly clean map up as time goes by requests are processed and can be discarded
	/** A cache of results based on search */
	TMap<FString, FSearchResults> SearchResultsCache;

	TMap<FString, FName> CategoryToIconMap;

	/** -1 if not set, otherwise index into Suggestions */
	int32 SelectedSuggestion;

	/** Entry that was actually clicked or pressed enter on. NULLs out when menu closes */
	TSharedPtr<FSearchEntry> EntryClicked;

	/** to prevent recursive calls in UI callback */
	bool bIgnoreUIUpdate; 
};