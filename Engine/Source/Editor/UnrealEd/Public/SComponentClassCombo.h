// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

typedef TSharedPtr<class FComponentClassComboEntry> FComponentClassComboEntryPtr;

//////////////////////////////////////////////////////////////////////////


DECLARE_DELEGATE_OneParam(FComponentClassSelected, TSubclassOf<UActorComponent>);

class FComponentClassComboEntry: public TSharedFromThis<FComponentClassComboEntry>
{
public:
	FComponentClassComboEntry( const FString& InHeadingText, TSubclassOf<UActorComponent> InComponentClass )
		: ComponentClass(InComponentClass)
		, HeadingText(InHeadingText)
	{}

	FComponentClassComboEntry( const FString& InHeadingText )
		: ComponentClass(NULL)
		, HeadingText(InHeadingText)
	{}

	FComponentClassComboEntry()
		: ComponentClass(NULL)
	{}


	TSubclassOf<UActorComponent> GetComponentClass() const { return ComponentClass; }

	const FString& GetHeadingText() const { return HeadingText; }

	bool IsHeading() const
	{
		return (ComponentClass==NULL && !HeadingText.IsEmpty());
	}
	bool IsSeparator() const
	{
		return (ComponentClass==NULL && HeadingText.IsEmpty());
	}
	bool IsClass() const
	{
		return (ComponentClass!=NULL);
	}

private:
	TSubclassOf<UActorComponent> ComponentClass;

	FString HeadingText;
};

//////////////////////////////////////////////////////////////////////////

class UNREALED_API SComponentClassCombo : public SComboButton
{
public:
	SLATE_BEGIN_ARGS( SComponentClassCombo )
	{}

		SLATE_EVENT( FComponentClassSelected, OnComponentClassSelected )

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Clear the current combo list selection */
	void ClearSelection();

	/**
	 * Updates the filtered list of component names.
	 * @param InSearchText The search text from the search control.
	 */
	void GenerateFilteredComponentList(const FString& InSearchText);

	FText GetCurrentSearchString() const;

	/**
	 * Called when the user changes the text in the search box.
	 * @param InSearchText The new search string.
	 */
	void OnSearchBoxTextChanged( const FText& InSearchText );

	/** Callback when the user commits the text in the searchbox */
	void OnSearchBoxTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	void OnAddComponentSelectionChanged( FComponentClassComboEntryPtr InItem, ESelectInfo::Type SelectInfo );

	TSharedRef<ITableRow> GenerateAddComponentRow( FComponentClassComboEntryPtr Entry, const TSharedRef<STableViewBase> &OwnerTable ) const;

	/** Update list of component classes */
	void UpdateComponentClassList();

	/** Returns a component name without the substring "Component" and sanitized for display */
	static FString GetSanitizedComponentName( UClass* ComponentClass );

private:

	FComponentClassSelected OnComponentClassSelected;

	/** List of component class names used by combo box */
	TArray<FComponentClassComboEntryPtr> ComponentClassList;

	/** List of component class names, filtered by the current search string */
	TArray<FComponentClassComboEntryPtr> FilteredComponentClassList;

	/** The current search string */
	FText CurrentSearchString;

	/** The search box control - part of the combo drop down */
	TSharedPtr<SSearchBox> SearchBox;

	/** The component list control - part of the combo drop down */
	TSharedPtr< SListView<FComponentClassComboEntryPtr> > ComponentClassListView;

	/** Cached selection index used to skip over unselectable items */
	int32 PrevSelectedIndex;
};
