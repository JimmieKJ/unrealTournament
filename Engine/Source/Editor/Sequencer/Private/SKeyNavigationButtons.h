// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneCommonHelpers.h"
#include "ScopedTransaction.h"
#include "MovieSceneSection.h"

#define LOCTEXT_NAMESPACE "Arse"

/**
 * A widget for navigating between keys on a sequencer track
 */
class SKeyNavigationButtons
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SKeyNavigationButtons){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FSequencerDisplayNode>& InDisplayNode)
	{
		DisplayNode = InDisplayNode;

		const FSlateBrush* NoBorder = FEditorStyle::GetBrush( "NoBorder" );

		TAttribute<FLinearColor> HoverTint(this, &SKeyNavigationButtons::GetHoverTint);

		ChildSlot
		[
			SNew(SHorizontalBox)
			
			// Previous key slot
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(3, 0, 0, 0)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(NoBorder)
				.ColorAndOpacity(HoverTint)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton")
					.ToolTipText(LOCTEXT("PreviousKeyButton", "Set the time to the previous key"))
					.OnClicked(this, &SKeyNavigationButtons::OnPreviousKeyClicked)
					.ForegroundColor( FSlateColor::UseForeground() )
					.ContentPadding(0)
					.IsFocusable(false)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
						.Text(FText::FromString(FString(TEXT("\xf060"))) /*fa-arrow-left*/)
					]
				]
			]
			// Add key slot
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(NoBorder)
				.ColorAndOpacity(HoverTint)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton")
					.ToolTipText(LOCTEXT("AddKeyButton", "Add a new key at the current time"))
					.OnClicked(this, &SKeyNavigationButtons::OnAddKeyClicked)
					.ForegroundColor( FSlateColor::UseForeground() )
					.ContentPadding(0)
					.IsFocusable(false)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
						.Text(FText::FromString(FString(TEXT("\xf055"))) /*fa-plus-circle*/)
					]
				]
			]
			// Next key slot
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(NoBorder)
				.ColorAndOpacity(HoverTint)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton")
					.ToolTipText(LOCTEXT("NextKeyButton", "Set the time to the next key"))
					.OnClicked(this, &SKeyNavigationButtons::OnNextKeyClicked)
					.ContentPadding(0)
					.ForegroundColor( FSlateColor::UseForeground() )
					.IsFocusable(false)
					[
						SNew(STextBlock)
						.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.7"))
						.Text(FText::FromString(FString(TEXT("\xf061"))) /*fa-arrow-right*/)
					]
				]
			]
		];
	}

	/** Handles the previous key button being clicked. */
	//FReply OnPreviousKeyClicked();

	/** Handles the next key button being clicked. */
	//FReply OnNextKeyClicked();

	/** Handles the add key button being clicked. */
	//FReply OnAddKeyClicked();


	FLinearColor GetHoverTint() const
	{
		return DisplayNode->IsHovered() ? FLinearColor(1,1,1,0.9f) : FLinearColor(1,1,1,0.4f);
	}

	FReply OnPreviousKeyClicked()
	{
		FSequencer& Sequencer = DisplayNode->GetSequencer();
		float ClosestPreviousKeyDistance = MAX_FLT;
		float CurrentTime = Sequencer.GetCurrentLocalTime(*Sequencer.GetFocusedMovieSceneSequence());
		float PreviousTime = 0;
		bool PreviousKeyFound = false;

		TSet<TSharedPtr<IKeyArea>> KeyAreas;
		SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);
		for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
		{
			for (FKeyHandle& KeyHandle : KeyArea->GetUnsortedKeyHandles())
			{
				float KeyTime = KeyArea->GetKeyTime(KeyHandle);
				if (KeyTime < CurrentTime && CurrentTime - KeyTime < ClosestPreviousKeyDistance)
				{
					PreviousTime = KeyTime;
					ClosestPreviousKeyDistance = CurrentTime - KeyTime;
					PreviousKeyFound = true;
				}
			}
		}

		if (PreviousKeyFound)
		{
			Sequencer.SetGlobalTime(PreviousTime);
		}
		return FReply::Handled();
	}


	FReply OnNextKeyClicked()
	{
		FSequencer& Sequencer = DisplayNode->GetSequencer();
		float ClosestNextKeyDistance = MAX_FLT;
		float CurrentTime = Sequencer.GetCurrentLocalTime(*Sequencer.GetFocusedMovieSceneSequence());
		float NextTime = 0;
		bool NextKeyFound = false;

		TSet<TSharedPtr<IKeyArea>> KeyAreas;
		SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);
		for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
		{
			for (FKeyHandle& KeyHandle : KeyArea->GetUnsortedKeyHandles())
			{
				float KeyTime = KeyArea->GetKeyTime(KeyHandle);
				if (KeyTime > CurrentTime && KeyTime - CurrentTime < ClosestNextKeyDistance)
				{
					NextTime = KeyTime;
					ClosestNextKeyDistance = KeyTime - CurrentTime;
					NextKeyFound = true;
				}
			}
		}

		if (NextKeyFound)
		{
			Sequencer.SetGlobalTime(NextTime);
		}

		return FReply::Handled();
	}


	FReply OnAddKeyClicked()
	{
		FSequencer& Sequencer = DisplayNode->GetSequencer();
		float CurrentTime = Sequencer.GetCurrentLocalTime(*Sequencer.GetFocusedMovieSceneSequence());

		TSet<TSharedPtr<IKeyArea>> KeyAreas;
		SequencerHelpers::GetAllKeyAreas(DisplayNode, KeyAreas);

		TArray<UMovieSceneSection*> KeyAreaSections;
		for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
		{
			UMovieSceneSection* OwningSection = KeyArea->GetOwningSection();
			KeyAreaSections.Add(OwningSection);
		}

		UMovieSceneSection* NearestSection = MovieSceneHelpers::FindNearestSectionAtTime(KeyAreaSections, CurrentTime);
		if (!NearestSection)
		{
			return FReply::Unhandled();
		}

		FScopedTransaction Transaction(LOCTEXT("AddKeys", "Add keys at current time"));
		for (TSharedPtr<IKeyArea> KeyArea : KeyAreas)
		{
			UMovieSceneSection* OwningSection = KeyArea->GetOwningSection();
			if (OwningSection == NearestSection)
			{
				OwningSection->SetFlags(RF_Transactional);
				if (OwningSection->TryModify())
				{
					KeyArea->AddKeyUnique(CurrentTime, Sequencer.GetKeyInterpolation());
				}
			}
		}

		Sequencer.UpdatePlaybackRange();

		return FReply::Handled();
	}

	TSharedPtr<FSequencerDisplayNode> DisplayNode;
};

#undef LOCTEXT_NAMESPACE