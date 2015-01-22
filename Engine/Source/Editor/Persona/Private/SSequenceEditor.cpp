// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "PersonaPrivatePCH.h"

#include "SSequenceEditor.h"
#include "GraphEditor.h"
#include "GraphEditorModule.h"
#include "Editor/Kismet/Public/SKismetInspector.h"
#include "SAnimationScrubPanel.h"
#include "SAnimNotifyPanel.h"
#include "SAnimCurvePanel.h"
#include "SAnimTrackCurvePanel.h"

#define LOCTEXT_NAMESPACE "AnimSequenceEditor"

//////////////////////////////////////////////////////////////////////////
// SSequenceEditor

void SSequenceEditor::Construct(const FArguments& InArgs)
{
	SequenceObj = InArgs._Sequence;
	check(SequenceObj);

	SAnimEditorBase::Construct( SAnimEditorBase::FArguments()
		.Persona(InArgs._Persona)
		);

	PersonaPtr.Pin()->RegisterOnPostUndo(FPersona::FOnPostUndo::CreateSP( this, &SSequenceEditor::PostUndo ) );
	PersonaPtr.Pin()->RegisterOnChangeTrackCurves(FPersona::FOnTrackCurvesChanged::CreateSP( this, &SSequenceEditor::OnTrackCurveChanged ) );

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimNotifyPanel, SAnimNotifyPanel )
		.Persona(InArgs._Persona)
		.Sequence(SequenceObj)
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
		.OnSelectionChanged(this, &SAnimEditorBase::OnSelectionChanged)
	];

	EditorPanels->AddSlot()
	.AutoHeight()
	.Padding(0, 10)
	[
		SAssignNew( AnimCurvePanel, SAnimCurvePanel )
		.Persona(InArgs._Persona)
		.Sequence(SequenceObj)
		.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
		.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
		.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
		.InputMin(this, &SAnimEditorBase::GetMinInput)
		.InputMax(this, &SAnimEditorBase::GetMaxInput)
		.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
		.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
	];

	UAnimSequence * AnimSeq = Cast<UAnimSequence>(SequenceObj);
	if (AnimSeq)
	{
		EditorPanels->AddSlot()
		.AutoHeight()
		.Padding(0, 10)
		[
			SAssignNew(AnimTrackCurvePanel, SAnimTrackCurvePanel)
			.Persona(InArgs._Persona)
			.Sequence(AnimSeq)
			.WidgetWidth(S2ColumnWidget::DEFAULT_RIGHT_COLUMN_WIDTH)
			.ViewInputMin(this, &SAnimEditorBase::GetViewMinInput)
			.ViewInputMax(this, &SAnimEditorBase::GetViewMaxInput)
			.InputMin(this, &SAnimEditorBase::GetMinInput)
			.InputMax(this, &SAnimEditorBase::GetMaxInput)
			.OnSetInputViewRange(this, &SAnimEditorBase::SetInputViewRange)
			.OnGetScrubValue(this, &SAnimEditorBase::GetScrubValue)
		];
	}
}

SSequenceEditor::~SSequenceEditor()
{
	if (PersonaPtr.IsValid())
	{
		PersonaPtr.Pin()->UnregisterOnPostUndo(this);
		PersonaPtr.Pin()->UnregisterOnChangeTrackCurves(this);
	}
}

void SSequenceEditor::PostUndo()
{
	TSharedPtr<FPersona> SharedPersona = PersonaPtr.Pin();
	if(SharedPersona.IsValid())
	{
		SharedPersona->SetPreviewAnimationAsset(SequenceObj);
	}

	if( SequenceObj )
	{
		SetInputViewRange(0, SequenceObj->SequenceLength);

		AnimNotifyPanel->Update();
		AnimCurvePanel->UpdatePanel();
		if (AnimTrackCurvePanel.IsValid())
		{
			AnimTrackCurvePanel->UpdatePanel();
		}
	}
}

void SSequenceEditor::OnTrackCurveChanged()
{
	if (AnimTrackCurvePanel.IsValid())
	{
		AnimTrackCurvePanel->UpdatePanel();
	}
}

#undef LOCTEXT_NAMESPACE
