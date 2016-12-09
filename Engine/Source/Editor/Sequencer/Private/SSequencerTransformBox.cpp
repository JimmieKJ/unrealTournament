// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SSequencerTransformBox.h"
#include "Sequencer.h"
#include "SequencerSettings.h"
#include "SequencerCommonHelpers.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "Sequencer"

void SSequencerTransformBox::Construct(const FArguments& InArgs, const TSharedRef<FSequencer>& InSequencer, USequencerSettings& InSettings, const TSharedRef<INumericTypeInterface<float>>& InNumericTypeInterface)
{
	SequencerPtr = InSequencer;
	Settings = &InSettings;
	NumericTypeInterface = InNumericTypeInterface;
	
	DeltaTime = 1.f;

	// Initialize to 10 frames if showing frame numbers
	float TimeSnapInterval = SequencerPtr.Pin()->GetSettings()->GetTimeSnapInterval();
	if (SequencerSnapValues::IsTimeSnapIntervalFrameRate(TimeSnapInterval))
	{	
		float FrameRate = 1.0f / TimeSnapInterval;
		DeltaTime = SequencerHelpers::FrameToTime(10, FrameRate);
	}

	const FDockTabStyle* GenericTabStyle = &FCoreStyle::Get().GetWidgetStyle<FDockTabStyle>("Docking.Tab");
	const FButtonStyle* const CloseButtonStyle = &GenericTabStyle->CloseButtonStyle;

	ChildSlot
	[
		SAssignNew(Border, SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			.Padding(6.0f)
			.Visibility(EVisibility::Collapsed)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.Text(LOCTEXT("PlusLabel", "+"))
							.OnClicked(this, &SSequencerTransformBox::OnPlusButtonClicked)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.Text(LOCTEXT("MinusLabel", "-"))
							.OnClicked(this, &SSequencerTransformBox::OnMinusButtonClicked)
					]

				+ SHorizontalBox::Slot()
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					.AutoWidth()
					[
						SAssignNew(EntryBox, SNumericEntryBox<float>)
							.MinDesiredValueWidth(32.0f)
							.TypeInterface(NumericTypeInterface)
							.OnValueCommitted(this, &SSequencerTransformBox::OnDeltaChanged)
							.Value_Lambda([this](){ return DeltaTime; })
					]

				+ SHorizontalBox::Slot()
				.Padding(3.0f, 0.0f, 0.0f, 0.0f)
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle( CloseButtonStyle )
					.OnClicked( this, &SSequencerTransformBox::OnCloseButtonClicked )
					.ContentPadding( 0 )
					[
						SNew(SSpacer)
						.Size( CloseButtonStyle->Normal.ImageSize )
					]
				]
			]
	];
}


void SSequencerTransformBox::ToggleVisibility()
{
	FSlateApplication& SlateApplication = FSlateApplication::Get();

	if (Border->GetVisibility() == EVisibility::Visible)
	{
		SlateApplication.SetAllUserFocus(LastFocusedWidget.Pin(), EFocusCause::Navigation);
		Border->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		Border->SetVisibility(EVisibility::Visible);
		LastFocusedWidget = SlateApplication.GetUserFocusedWidget(0);
		SlateApplication.SetAllUserFocus(EntryBox, EFocusCause::Navigation);
	}
}

void SSequencerTransformBox::OnDeltaChanged(float Value, ETextCommit::Type CommitType)
{
	if (CommitType != ETextCommit::OnEnter)
	{
		return;
	}

	DeltaTime = Value;
}

FReply SSequencerTransformBox::OnPlusButtonClicked()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	Sequencer->TransformSelectedKeysAndSections(DeltaTime);

	return FReply::Handled();
}

FReply SSequencerTransformBox::OnMinusButtonClicked()
{
	TSharedPtr<FSequencer> Sequencer = SequencerPtr.Pin();

	Sequencer->TransformSelectedKeysAndSections(-DeltaTime);

	return FReply::Handled();
}

FReply SSequencerTransformBox::OnCloseButtonClicked()
{
	ToggleVisibility();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
