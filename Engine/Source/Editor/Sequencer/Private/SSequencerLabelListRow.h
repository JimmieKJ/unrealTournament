// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class FSequencerLabelTreeNode
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InLabel The node's label.
	 */
	FSequencerLabelTreeNode(const FString& InLabel)
		: Label(InLabel)
	{ }

public:

	/**
	 * Adds a child label node to this node.
	 *
	 * @param The child node to add.
	 */
	void AddChild(const TSharedPtr<FSequencerLabelTreeNode>& Child)
	{
		Children.Add(Child);
	}

	/** Clears the collection of child nodes. */
	void ClearChildren()
	{
		Children.Reset();
	}

	/**
	 * Gets the child nodes.
	 *
	 * @return Child nodes.
	 */
	const TArray<TSharedPtr<FSequencerLabelTreeNode>>& GetChildren()
	{
		return Children;
	}

	/**
	 * Gets the node's process information.
	 *
	 * @return The process information.
	 */
	const FString& GetLabel() const
	{
		return Label;
	}

private:

	/** Holds the child label nodes. */
	TArray<TSharedPtr<FSequencerLabelTreeNode>> Children;

	/** Holds the label. */
	FString Label;
};


#define LOCTEXT_NAMESPACE "SSequencerLabelListRow"

/**
 * Implements a row widget for the label browser tree view.
 */
class SSequencerLabelListRow
	: public STableRow<TSharedPtr<FSequencerLabelTreeNode>>
{
public:

	SLATE_BEGIN_ARGS(SSequencerLabelListRow) { }
		SLATE_ARGUMENT(TSharedPtr<FSequencerLabelTreeNode>, Node)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		Node = InArgs._Node;

		ChildSlot
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				// folder icon
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SImage) 
							.Image(this, &SSequencerLabelListRow::HandleFolderIconImage)
							.ColorAndOpacity(this, &SSequencerLabelListRow::HandleFolderIconColor)
					]

				// folder name
				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.Padding(0.0f, 2.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
							.Text(
								Node->GetLabel().IsEmpty()
									? LOCTEXT("AllTracksLabel", "All Tracks")
									: FText::FromString(Node->GetLabel())
							)
					]

				// edit icon
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
							.Image(FCoreStyle::Get().GetBrush(TEXT("EditableLabel.EditIcon")))
							.Visibility(this, &SSequencerLabelListRow::HandleEditIconVisibility)
					]
			];

		STableRow<TSharedPtr<FSequencerLabelTreeNode>>::ConstructInternal(
			STableRow<TSharedPtr<FSequencerLabelTreeNode>>::FArguments()
				.ShowSelection(false)
				.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow"),
			InOwnerTableView
		);
	}

private:

	EVisibility HandleEditIconVisibility() const
	{
		return IsHovered()
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	const FSlateBrush* HandleFolderIconImage() const
	{
		static const FSlateBrush* FolderOpenBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
		static const FSlateBrush* FolderClosedBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");

		return IsItemExpanded()
			? FolderOpenBrush
			: FolderClosedBrush;
	}

	FSlateColor HandleFolderIconColor() const
	{
		// TODO sequencer: gmp: allow folder color customization
		return FLinearColor::Gray;
	}

private:

	/** Holds the label node. */
	TSharedPtr<FSequencerLabelTreeNode> Node;
};


#undef LOCTEXT_NAMESPACE
