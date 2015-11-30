// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "MovieScene.h"
#include "SSequencerLabelEditorListRow.h"


#define LOCTEXT_NAMESPACE "SSequencerLabelEditor"


/* SSequencerLabelEditor interface
 *****************************************************************************/

void SSequencerLabelEditor::Construct(const FArguments& InArgs, UMovieScene* InMovieScene, const FGuid& InObjectId)
{
	MovieScene = InMovieScene;
	ObjectId = InObjectId;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("LabelAs", "Label as:"))
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(FilterBox, SEditableTextBox)
							.MinDesiredWidth(144.0f)
							.OnTextChanged(this, &SSequencerLabelEditor::HandleFilterBoxTextChanged)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
							.IsEnabled(this, &SSequencerLabelEditor::HandleCreateNewLabelButtonIsEnabled)
							.OnClicked(this, &SSequencerLabelEditor::HandleCreateNewLabelButtonClicked)
							.Content()
							[
								SNew(STextBlock)
									.Text(LOCTEXT("CreateNewLabelButton", "Create New"))
							]
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SAssignNew(LabelListView, SListView<TSharedPtr<FString>>)
					.ItemHeight(20.0f)
					.ListItemsSource(&LabelList)
					.OnGenerateRow(this, &SSequencerLabelEditor::HandleLabelListViewGenerateRow)
					.SelectionMode(ESelectionMode::None)
			]
	];

	ReloadLabelList(true);
}


/* SSequencerLabelEditor implementation
 *****************************************************************************/

void SSequencerLabelEditor::ReloadLabelList(bool FullyReload)
{
	// reload label list
	if (FullyReload)
	{
		AvailableLabels.Reset();
		MovieScene->GetAllLabels(AvailableLabels);
	}

	// filter label list
	TArray<FString> Filters;
	FilterBox->GetText().ToString().ParseIntoArray(Filters, TEXT(" "));

	LabelList.Reset();

	for (const auto& Label : AvailableLabels)
	{
		bool Matched = true;

		for (const auto& Filter : Filters)
		{
			if (!Label.Contains(Filter))
			{
				Matched = false;
				break;
			}
		}

		if (Matched)
		{
			LabelList.Add(MakeShareable(new FString(Label)));
		}
	}

	// refresh list view
	LabelListView->RequestListRefresh();
}


/* SSequencerLabelEditor callbacks
 *****************************************************************************/

FReply SSequencerLabelEditor::HandleCreateNewLabelButtonClicked()
{
	MovieScene->GetObjectLabels(ObjectId).Strings.AddUnique(FilterBox->GetText().ToString());
	FilterBox->SetText(FText::GetEmpty());

	return FReply::Handled();
}


bool SSequencerLabelEditor::HandleCreateNewLabelButtonIsEnabled() const
{
	FString FilterString = FilterBox->GetText().ToString();
	FilterString.Trim();

	return !FilterString.IsEmpty() && !MovieScene->LabelExists(FilterString);
}


void SSequencerLabelEditor::HandleFilterBoxTextChanged(const FText& NewText)
{
	ReloadLabelList(false);
}


TSharedRef<ITableRow> SSequencerLabelEditor::HandleLabelListViewGenerateRow(TSharedPtr<FString> Label, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SSequencerLabelEditorListRow, OwnerTable)
		.HighlightText(this, &SSequencerLabelEditor::HandleLabelListViewRowHighlightText)
		.IsChecked(this, &SSequencerLabelEditor::HandleLabelListViewRowIsChecked, Label)
		.Label(Label)
		.OnCheckStateChanged(this, &SSequencerLabelEditor::HandleLabelListViewRowCheckedStateChanged, Label);
}


void SSequencerLabelEditor::HandleLabelListViewRowCheckedStateChanged(ECheckBoxState State, TSharedPtr<FString> Label)
{
	FMovieSceneTrackLabels& Labels = MovieScene->GetObjectLabels(ObjectId);

	if (State == ECheckBoxState::Checked)
	{
		Labels.Strings.AddUnique(*Label);
	}
	else
	{
		Labels.Strings.Remove(*Label);
	}
}


FText SSequencerLabelEditor::HandleLabelListViewRowHighlightText() const
{
	return FilterBox->GetText();
}


ECheckBoxState SSequencerLabelEditor::HandleLabelListViewRowIsChecked(TSharedPtr<FString> Label) const
{
	const FMovieSceneTrackLabels& Labels = MovieScene->GetObjectLabels(ObjectId);
	return Labels.Strings.Contains(*Label)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}


#undef LOCTEXT_NAMESPACE
