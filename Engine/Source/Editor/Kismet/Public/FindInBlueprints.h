// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

typedef TSharedPtr<class FFindInBlueprintsResult> FSearchResult;
typedef STreeView<FSearchResult>  STreeViewType;

/* Item that matched the search results */
class FFindInBlueprintsResult : public TSharedFromThis< FFindInBlueprintsResult >
{
public: 
	/* Create a root */
	FFindInBlueprintsResult(const FText& InDisplayText);
	
	/* Create a listing for a search result*/
	FFindInBlueprintsResult(const FText& InDisplayText, TSharedPtr<FFindInBlueprintsResult> InParent);

	virtual ~FFindInBlueprintsResult() {}
	
	/* Called when user clicks on the search item */
	virtual FReply OnClick();

	/* Get Category for this search result */
	virtual FText GetCategory() const;

	/* Create an icon to represent the result */
	virtual TSharedRef<SWidget>	CreateIcon() const;

	/** Finalizes any content for the search data that was unsafe to do on a separate thread */
	virtual void FinalizeSearchData() {};

	/* Gets the comment on this node if any */
	FString GetCommentText() const;

	/** gets the blueprint housing all these search results */
	UBlueprint* GetParentBlueprint() const;

	/**
	 * Extracts the content of a JsonNode, building the sub-hierarchy for display
	 *
	 * @param InJsonNode		Node to extract data from
	 * @param InTokens			Tokens to match searchable content against
	 * @return					Returns TRUE if the content matches search results and was extracted
	 */
	virtual bool ExtractContent(TSharedPtr< FJsonObject > InJsonNode, const TArray<FString>& InTokens) { return ExtractContent(InJsonNode, InTokens, NULL); };

	/**
	 * Extracts the content of a JsonNode, building the sub-hierarchy for display
	 *
	 * @param InJsonNode		Node to extract data from
	 * @param InTokens			Tokens to match searchable content against
	 * @param InParentOverride	Parent search result to attach sub-content to
	 * @return					Returns TRUE if the content matches search results and was extracted
	 */
	virtual bool ExtractContent(TSharedPtr< FJsonObject > InJsonNode, const TArray<FString>& InTokens, TSharedPtr< FFindInBlueprintsResult > InParentOverride);

	/**
	 * Parses search info for specific data important for displaying the search result in an easy to understand format
	 *
	 * @param	InTokens		The search tokens to check results against
	 * @param	InKey			This is the tag for the data, describing what it is so special handling can occur if needed
	 * @param	InValue			Compared against search query to see if it passes the filter, sometimes data is rejected because it is deemed unsearchable
	 * @param	InParent		The parent search result
	 *
	 * @return					TRUE if the item passes the search filter or is otherwise accepted
	 */
	virtual bool ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent);

	/**
	 * Adds extra search info, anything that does not have a predestined place in the search result. Adds a sub-item to the searches and formats its description so the tag displays
	 *
	 * @param	InKey			This is the tag for the data, describing what it is so special handling can occur if needed
	 * @param	InValue			Compared against search query to see if it passes the filter, sometimes data is rejected because it is deemed unsearchable
	 * @param	InParent		The parent search result
	 */
	void AddExtraSearchInfo(FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParent);

	/**
	 * Iterates through all this node's children, and tells the tree view to expand them
	 */
	void ExpandAllChildren( TSharedPtr< STreeView< TSharedPtr< FFindInBlueprintsResult > > > InTreeView );

	/** Returns the display string for the row */
	FText GetDisplayString() const;

protected:

	/**
	 * Extracts the details of a JsonValue in the search results, pushing arrays into sub-objects correctly, breaking up objects, and passing on the rest of the types
	 *
	 * @param	InTokens				The search tokens to check results against
	 * @param	InKey					This is the tag for the data, describing what it is so special handling can occur if needed
	 * @param	InValue					Has all the data for this object that needs to be deciphered or broken up into base types to be deciphered
	 * @param	InParentOverride		The parent to attach all children data to, can be NULL.
	 */
	bool ExtractJsonValue(const TArray<FString>& InTokens, FText InKey, TSharedPtr< FJsonValue > InValue, TSharedPtr< FFindInBlueprintsResult > InParentOverride);

public:
	/*Any children listed under this category */
	TArray< TSharedPtr<FFindInBlueprintsResult> > Children;

	/*If it exists it is the blueprint*/
	TWeakPtr<FFindInBlueprintsResult> Parent;

	/*The display text for this item */
	FText DisplayText;

	/** Display text for comment information */
	FString CommentText;
};

/** Graph nodes use this class to store their data */
class FFindInBlueprintsGraphNode : public FFindInBlueprintsResult
{
public:
	FFindInBlueprintsGraphNode(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent);
	virtual ~FFindInBlueprintsGraphNode() {}

	/** FFindInBlueprintsResult Interface */
	virtual FReply OnClick() override;
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual bool ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParentOverride = NULL) override;
	virtual bool ExtractContent(TSharedPtr< FJsonObject > InJsonNode, const TArray<FString>& InTokens) override;
	virtual FText GetCategory() const override;
	virtual void FinalizeSearchData() override;
	/** End FFindInBlueprintsResult Interface */

private:
	/** The Node Guid to find when jumping to the node */
	FGuid NodeGuid;

	/** The glyph brush for this node */
	const struct FSlateBrush* GlyphBrush;

	/** The glyph color for this node */
	FLinearColor GlyphColor;

	/*The class this item refers to */
	UClass* Class;

	/*The class name this item refers to */
	FString ClassName;
};

/** Pins use this class to store their data */
class FFindInBlueprintsPin : public FFindInBlueprintsResult
{
public:
	FFindInBlueprintsPin(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent, FString InSchemaName);
	virtual ~FFindInBlueprintsPin() {}

	/** FFindInBlueprintsResult Interface */
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual bool ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParentOverride = NULL) override;
	virtual FText GetCategory() const override;
	virtual void FinalizeSearchData() override;
	/** End FFindInBlueprintsResult Interface */
protected:
	/** The name of the schema this pin exists under */
	FString SchemaName;

	/** The pin that this search result refers to */
	FEdGraphPinType PinType;

	/** The Pin's sub-category name */
	FString PinSubCategory;

	/** Pin's icon color */
	FSlateColor IconColor;
};

/** Property data is stored here */
class FFindInBlueprintsProperty : public FFindInBlueprintsResult
{
public:
	FFindInBlueprintsProperty(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent);
	virtual ~FFindInBlueprintsProperty() {}

	/** FFindInBlueprintsResult Interface */
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual bool ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParentOverride = NULL) override;
	virtual FText GetCategory() const override;
	virtual void FinalizeSearchData() override;
	/** End FFindInBlueprintsResult Interface */
protected:
	/** The pin that this search result refers to */
	FEdGraphPinType PinType;

	/** The default value of a property as a string */
	FString DefaultValue;

	/** The Property's sub-category name, for the pin display type */
	FString PinSubCategory;

	/** TRUE if the property is an SCS_Component */
	bool bIsSCSComponent;
};

/** Graphs, such as functions and macros, are stored here */
class FFindInBlueprintsGraph : public FFindInBlueprintsResult
{
public:
	FFindInBlueprintsGraph(const FText& InValue, TSharedPtr<FFindInBlueprintsResult> InParent, EGraphType InGraphType);
	virtual ~FFindInBlueprintsGraph() {}

	/** FFindInBlueprintsResult Interface */
	virtual FReply OnClick() override;
	virtual TSharedRef<SWidget>	CreateIcon() const override;
	virtual bool ParseSearchInfo(const TArray<FString> &InTokens, FString InKey, FText InValue, TSharedPtr< FFindInBlueprintsResult > InParentOverride = NULL) override;
	virtual FText GetCategory() const override;
	virtual bool ExtractContent(TSharedPtr< FJsonObject > InJsonNode, const TArray<FString>& InTokens) override;
	/** End FFindInBlueprintsResult Interface */
protected:

	/** The type of graph this represents */
	EGraphType GraphType;
};

/*Widget for searching for (functions/events) across all blueprints or just a single blueprint */
class SFindInBlueprints: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SFindInBlueprints ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class FBlueprintEditor> InBlueprintEditor);
	~SFindInBlueprints();

	/** Focuses this widget's search box, and changes the mode as well, and optionally the search terms */
	void FocusForUse(bool bSetFindWithinBlueprint, FString NewSearchTerms = FString(), bool bSelectFirstResult = false);

	/** Called when caching Blueprints is complete, if this widget initiated the indexing */
	void OnCacheComplete();
private:
	/** Processes results of the ongoing async stream search */
	EActiveTimerReturnType UpdateSearchResults( double InCurrentTime, float InDeltaTime );

	/** Register any Find-in-Blueprint commands */
	void RegisterCommands();

	/*Called when user changes the text they are searching for */
	void OnSearchTextChanged(const FText& Text);

	/*Called when user changes commits text to the search box */
	void OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	/** Called when the find mode checkbox is hit */
	void OnFindModeChanged(ECheckBoxState CheckState);

	/** Called to check what the find mode is for the checkbox */
	ECheckBoxState OnGetFindModeChecked() const;

	/* Get the children of a row */
	void OnGetChildren( FSearchResult InItem, TArray< FSearchResult >& OutChildren );

	/* Called when user double clicks on a new result */
	void OnTreeSelectionDoubleClicked( FSearchResult Item );

	/* Called when a new row is being generated */
	TSharedRef<ITableRow> OnGenerateRow(FSearchResult InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Begins the search based on the SearchValue */
	void InitiateSearch();
	
	/* Find any results that contain all of the tokens, however, only in find within blueprints */
	void MatchTokensWithinBlueprint(const TArray<FString>& Tokens);

	/** Launches a thread for streaming more content into the results widget */
	void LaunchStreamThread(const TArray<FString>& InTokens);

	/** Returns the percent complete on the search for the progress bar */
	TOptional<float> GetPercentCompleteSearch() const;

	/** Returns the progress bar visiblity */
	EVisibility GetSearchbarVisiblity() const;

	/** Adds the "cache" bar at the bottom of the Find-in-Blueprints widget, to notify the user that the search is incomplete */
	void ConditionallyAddCacheBar();

	/** Callback to remove the "cache" bar when a button is pressed */
	FReply OnRemoveCacheBar();

	/** Callback to return the cache bar's display text, informing the user of the situation */
	FText GetUncachedBlueprintWarningText() const;

	/** Callback to return the cache bar's current indexing Blueprint name */
	FText GetCurrentCacheBlueprintName() const;

	/** Callback to cache all uncached Blueprints */
	FReply OnCacheAllBlueprints();

	/** Callback to cancel the caching process */
	FReply OnCancelCacheAll();

	/** Retrieves the current index of the Blueprint caching process */
	int32 GetCurrentCacheIndex() const;

	/** Gets the percent complete of the caching process */
	TOptional<float> GetPercentCompleteCache() const;

	/** Returns the visibility of the caching progress bar, visible when in progress, hidden when not */
	EVisibility GetCachingProgressBarVisiblity() const;

	/** Returns the visibility of the "Cache All" button, visible when not caching, collapsed when caching is in progress */
	EVisibility GetCacheAllButtonVisibility() const;

	/** Returns the caching bar's visibility, it goes invisible when there is nothing to be cached. The next search will remove this bar or make it visible again */
	EVisibility GetCachingBarVisibility() const;

	/** Returns the visibility of the caching Blueprint name, visible when in progress, collapsed when not */
	EVisibility GetCachingBlueprintNameVisiblity() const;

	/** Returns the visibility of the popup button that displays the list of Blueprints that failed to cache */
	EVisibility GetFailedToCacheListVisibility() const;

	/** Returns TRUE if Blueprint caching is in progress */
	bool IsCacheInProgress() const;

	/** Returns the color of the caching bar */
	FSlateColor GetCachingBarColor() const;

	/** Callback to build the context menu when right clicking in the tree */
	TSharedPtr<SWidget> OnContextMenuOpening();

	/** Helper function to select all items */
	void SelectAllItemsHelper(FSearchResult InItemToSelect);

	/** Callback when user attempts to select all items in the search results */
	void OnSelectAllAction();

	/** Callback when user attempts to copy their selection in the Find-in-Blueprints */
	void OnCopyAction();

private:
	/** Pointer back to the blueprint editor that owns us */
	TWeakPtr<class FBlueprintEditor> BlueprintEditorPtr;
	
	/* The tree view displays the results */
	TSharedPtr<STreeViewType> TreeView;

	/** The search text box */
	TSharedPtr<class SSearchBox> SearchTextField;
	
	/* This buffer stores the currently displayed results */
	TArray<FSearchResult> ItemsFound;

	/** In Find Within Blueprint mode, we need to keep a handle on the root result, because it won't show up in the tree */
	FSearchResult RootSearchResult;

	/* The string to highlight in the results */
	FText HighlightText;

	/* The string to search for */
	FString	SearchValue;

	/** Should we search within the current blueprint only (rather than all blueprints) */
	bool bIsInFindWithinBlueprintMode;

	/** Thread object that searches through Blueprint data on a separate thread */
	TSharedPtr< class FStreamSearch> StreamSearch;

	/** Vertical box, used to add and remove widgets dynamically */
	TWeakPtr< SVerticalBox > MainVerticalBox;

	/** Weak pointer to the cache bar slot, so it can be removed */
	TWeakPtr< SWidget > CacheBarSlot;
};