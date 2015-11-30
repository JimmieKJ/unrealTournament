// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FSequencerLabelTreeNode;


/**
 * Implements a widget for browsing sequencer track labels.
 */
class SSequencerLabelBrowser
	: public SCompoundWidget
{
public:

	typedef TSlateDelegates<FString>::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS(SSequencerLabelBrowser)
		: _OnSelectionChanged()
	{ }

		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SSequencerLabelBrowser();

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InModel The view model to use.
	 * @param InStyle The visual style to use for this widget.
	 * @param InTracer The message tracer.
	 */
	void Construct(const FArguments& InArgs/*, const FMessagingDebuggerModelRef& InModel*/);

protected:

	/** Reloads the list of processes. */
	void ReloadLabelList(bool FullyReload);

private:

	/** Callback for generating the label tree view's context menu. */
	TSharedPtr<SWidget> HandleLabelTreeViewContextMenuOpening();

	/** Callback for generating a row widget in the label tree view. */
	TSharedRef<ITableRow> HandleLabelTreeViewGenerateRow(TSharedPtr<FSequencerLabelTreeNode> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Callback for getting the children of a node in the label tree view. */
	void HandleLabelTreeViewGetChildren(TSharedPtr<FSequencerLabelTreeNode> Item, TArray<TSharedPtr<FSequencerLabelTreeNode>>& OutChildren);

	/** Callback for when the label tree view selection changed. */
	void HandleLabelTreeViewSelectionChanged(TSharedPtr<FSequencerLabelTreeNode> InItem, ESelectInfo::Type SelectInfo);

	/** Callback for executing the 'Set Remove label' context menu entry. */
	void HandleRemoveLabelMenuEntryExecute();

	/** Callback for checking whether 'Remove label' context menu entry can execute. */
	bool HandleRemoveLabelMenuEntryCanExecute() const;

	/** Callback for executing the 'Rename label' context menu entry. */
	void HandleRenameLabelMenuEntryExecute();

	/** Callback for checking whether 'Rename' context menu entry can execute. */
	bool HandleRenameLabelMenuEntryCanExecute() const;

	/** Callback for executing the 'Set Color' context menu entry. */
	void HandleSetColorMenuEntryExecute();

	/** Callback for checking whether 'Set Color' context menu entry can execute. */
	bool HandleSetColorMenuEntryCanExecute() const;

private:

	/** Holds the filtered list of processes running on the device. */
	TArray<TSharedPtr<FSequencerLabelTreeNode>> LabelList;

	/** Holds the label tree view. */
	TSharedPtr<STreeView<TSharedPtr<FSequencerLabelTreeNode>>> LabelTreeView;

	/** Delegate to invoke when the selected label changed. */
	FOnSelectionChanged OnSelectionChanged;
};
