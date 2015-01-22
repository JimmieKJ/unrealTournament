// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintGraphDefinitions.h"

DECLARE_DELEGATE_OneParam(FOnPinTypeChanged, const FEdGraphPinType&)


//////////////////////////////////////////////////////////////////////////
// SPinTypeSelector

typedef TSharedPtr<class UEdGraphSchema_K2::FPinTypeTreeInfo> FPinTypeTreeItem;
typedef STreeView<FPinTypeTreeItem> SPinTypeTreeView;

DECLARE_DELEGATE_ThreeParams(FGetPinTypeTree, TArray<FPinTypeTreeItem >&, bool, bool);

/** Widget for modifying the type for a variable or pin */
class KISMETWIDGETS_API SPinTypeSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SPinTypeSelector )
		: _TargetPinType()
		, _Schema(NULL)
		, _bAllowExec(false)
		, _bAllowWildcard(false)
		, _bAllowArrays(true)
		, _TreeViewWidth(300.f)
		, _TreeViewHeight(400.f)
		, _Font( FEditorStyle::GetFontStyle( TEXT("NormalFont") ) )
		{}
		SLATE_ATTRIBUTE( FEdGraphPinType, TargetPinType )
		SLATE_ARGUMENT( const UEdGraphSchema_K2*, Schema )
		SLATE_ARGUMENT( bool, bAllowExec )
		SLATE_ARGUMENT( bool, bAllowWildcard )
		SLATE_ARGUMENT( bool, bAllowArrays )
		SLATE_ATTRIBUTE( FOptionalSize, TreeViewWidth )
		SLATE_ATTRIBUTE( FOptionalSize, TreeViewHeight )
		SLATE_EVENT( FOnPinTypeChanged, OnPinTypePreChanged )
		SLATE_EVENT( FOnPinTypeChanged, OnPinTypeChanged )
		SLATE_ATTRIBUTE( FSlateFontInfo, Font )
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, FGetPinTypeTree GetPinTypeTreeFunc);

protected:
	/** Gets the icon (pin or array) for the type being manipulated */
	const FSlateBrush* GetTypeIconImage() const;

	/** Gets the type-specific color for the type being manipulated */
	FSlateColor GetTypeIconColor() const;

	/** Gets a succinct type description for the type being manipulated */
	virtual FText GetTypeDescription() const;

	TSharedPtr<SComboButton>		TypeComboButton;
	TSharedPtr<SSearchBox>			FilterTextBox;
	TSharedPtr<SPinTypeTreeView>	TypeTreeView;
	
	/** The pin attribute that we're modifying with this widget */
	TAttribute<FEdGraphPinType>		TargetPinType;

	/** Delegate that is called every time the pin type changes (before and after). */
	FOnPinTypeChanged			OnTypeChanged;
	FOnPinTypeChanged			OnTypePreChanged;

	/** Delegate for the type selector to retrieve the pin type tree (passed into the Construct so the tree can depend on the situation) */
	FGetPinTypeTree				GetPinTypeTree;

	/** Schema in charge of determining available types for this pin */
	UEdGraphSchema_K2*			Schema;

	/** Whether or not to allow exec type wires in the type selection */
	bool						bAllowExec;

	/** Whether or not to allow wildcard as an option in the type selection */
	bool						bAllowWildcard;

	/** Desired width of the tree view widget */
	TAttribute<FOptionalSize>	TreeViewWidth;

	/** Desired height of the tree view widget */
	TAttribute<FOptionalSize>	TreeViewHeight;

	/** Array checkbox support functions */
	ECheckBoxState IsArrayChecked() const;
	void OnArrayCheckStateChanged(ECheckBoxState NewState);

	/** Array containing the unfiltered list of all supported types this pin could possibly have */
	TArray<FPinTypeTreeItem>		TypeTreeRoot;
	/** Array containing a filtered list, according to the text in the searchbox */
	TArray<FPinTypeTreeItem>		FilteredTypeTreeRoot;

	/** Treeview support functions */
	virtual TSharedRef<ITableRow> GenerateTypeTreeRow(FPinTypeTreeItem InItem, const TSharedRef<STableViewBase>& OwnerTree);
	void OnTypeSelectionChanged(FPinTypeTreeItem Selection, ESelectInfo::Type SelectInfo);
	void GetTypeChildren(FPinTypeTreeItem InItem, TArray<FPinTypeTreeItem>& OutChildren);

	/** Reference to the menu content that's displayed when the type button is clicked on */
	TSharedPtr<SWidget> MenuContent;
	virtual TSharedRef<SWidget>	GetMenuContent();

	/** Type searching support */
	FText SearchText;
	void OnFilterTextChanged(const FText& NewText);
	void OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	/** Helper to generate the filtered list of types, based on the search string matching */
	bool GetChildrenMatchingSearch(const FString& SearchText, const TArray<FPinTypeTreeItem>& UnfilteredList, TArray<FPinTypeTreeItem>& OutFilteredList);

	/** 
	 * Determine the best icon to represent the given pin.
	 *
	 * @param PinType		The pin get the icon for.
	 * @param returns a brush that best represents the icon (or Kismet.VariableList.TypeIcon if none is available )
	 */
	const FSlateBrush* GetIconFromPin( const FEdGraphPinType& PinType ) const;

	/** Callback to get the tooltip text for the pin type combo box */
	FText GetToolTipForComboBoxType() const;

	/** Callback to get the tooltip for the array button widget */
	FText GetToolTipForArrayWidget() const;
};
