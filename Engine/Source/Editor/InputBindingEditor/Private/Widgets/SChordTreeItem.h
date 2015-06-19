// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * An item for the chord tree view
 */
struct FChordTreeItem
{
	// Note these are mutually exclusive
	TWeakPtr<FBindingContext> BindingContext;
	TSharedPtr<FUICommandInfo> CommandInfo;

	TSharedPtr<FBindingContext> GetBindingContext() { return BindingContext.Pin(); }

	bool IsContext() const { return BindingContext.IsValid(); }
	bool IsCommand() const { return CommandInfo.IsValid(); }
};


typedef STreeView< TSharedPtr<FChordTreeItem> > SChordTree;


/**
 * A widget which visualizes a command info      .              
 */
class SChordTreeItem
	: public SMultiColumnTableRow< TSharedPtr< FChordTreeItem > >
 {
public:

	SLATE_BEGIN_ARGS( SChordTreeItem ){}
	SLATE_END_ARGS()

 public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 * @param InOwnerTable The table that owns this tree item.
	 * @param InItem The actual item to be displayed.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedPtr<FChordTreeItem> InItem );

	/**
	 * Called to generate a widget for each column.
	 *
	 * @param ColumnName The name of the column that needs a widget.
	 */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

public:

	// SWidget interface

	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	virtual FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:

	/** Holds the tree item being visualized. */
	TSharedPtr<FChordTreeItem> TreeItem;
};
