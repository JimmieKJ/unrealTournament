// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


class SSequencerLabelEditor
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSequencerLabelEditor)
	{ }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InObjectId The unique identifier of the object whose labels are being edited.
	 */
	void Construct(const FArguments& InArgs, UMovieScene* InMovieScene, const FGuid& InObjectId);

protected:

	/**
	 * Reload the list of track labels.
	 *
	 * @param FullyReload Whether to fully reload entries, or only re-apply filtering.
	 */
	void ReloadLabelList(bool FullyReload);

private:

	/** Callback for clicking the 'Create new label' button. */
	FReply HandleCreateNewLabelButtonClicked();

	/** Callback for getting the enabled state of the 'Create new label' button. */
	bool HandleCreateNewLabelButtonIsEnabled() const;

	/** Callback for text changes in the filter text box. */
	void HandleFilterBoxTextChanged(const FText& NewText);

	/** Callback for generating a row widget in the device service list view. */
	TSharedRef<ITableRow> HandleLabelListViewGenerateRow(TSharedPtr<FString> Label, const TSharedRef<STableViewBase>& OwnerTable);

	/** Callback for changing the state of a list view row's check box. */
	void HandleLabelListViewRowCheckedStateChanged(ECheckBoxState State, TSharedPtr<FString> Label);

	/** Callback for getting the highlight text in session rows. */
	FText HandleLabelListViewRowHighlightText() const;

	/** Callback for determining whether the check box of a list view row is checked. */
	ECheckBoxState HandleLabelListViewRowIsChecked(TSharedPtr<FString> Label) const;

private:

	/** The list of available track labels. */
	TArray<FString> AvailableLabels;

	/** The label filter text box. */
	TSharedPtr<SEditableTextBox> FilterBox;

	/** The filtered list of track labels. */
	TArray<TSharedPtr<FString>> LabelList;

	/** Holds the list view for filtered track labels. */
	TSharedPtr<SListView<TSharedPtr<FString>>> LabelListView;

	/** The movie scene being edited. */
	UMovieScene* MovieScene;

	/** The identifier of the object being edited. */
	FGuid ObjectId;
};
