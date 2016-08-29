// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "SIntegralCurveKeyEditor.h"

#define LOCTEXT_NAMESPACE "IntegralCurveKeyEditor"

void SIntegralCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	IntermediateValue = InArgs._IntermediateValue;
	ChildSlot
	[
		SNew(SSpinBox<int32>)
		.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
		.MinValue(TOptional<int32>())
		.MaxValue(TOptional<int32>())
		.MaxSliderValue(TOptional<int32>())
		.MinSliderValue(TOptional<int32>())
		.Delta(1)
		.Value(this, &SIntegralCurveKeyEditor::OnGetKeyValue)
		.OnValueChanged(this, &SIntegralCurveKeyEditor::OnValueChanged)
		.OnValueCommitted(this, &SIntegralCurveKeyEditor::OnValueCommitted)
		.OnBeginSliderMovement(this, &SIntegralCurveKeyEditor::OnBeginSliderMovement)
		.OnEndSliderMovement(this, &SIntegralCurveKeyEditor::OnEndSliderMovement)
		.ClearKeyboardFocusOnCommit(true)
	];
}

void SIntegralCurveKeyEditor::OnBeginSliderMovement()
{
	GEditor->BeginTransaction(LOCTEXT("SetIntegralKey", "Set Integral Key Value"));
	OwningSection->SetFlags(RF_Transactional);
	OwningSection->TryModify();
}

void SIntegralCurveKeyEditor::OnEndSliderMovement(int32 Value)
{
	if (GEditor->IsTransactionActive())
	{
		GEditor->EndTransaction();
	}
}

int32 SIntegralCurveKeyEditor::OnGetKeyValue() const
{
	if ( IntermediateValue.IsSet() && IntermediateValue.Get().IsSet() )
	{
		return IntermediateValue.Get().GetValue();
	}

	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
	return Curve->Evaluate(CurrentTime);
}

void SIntegralCurveKeyEditor::OnValueChanged(int32 Value)
{
	if (OwningSection->TryModify())
	{
		float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());

		bool bKeyWillBeAdded = Curve->IsKeyHandleValid(Curve->FindKey(CurrentTime)) == false;
		if (bKeyWillBeAdded)
		{
			if (OwningSection->GetStartTime() > CurrentTime)
			{
				OwningSection->SetStartTime(CurrentTime);
			}
			if (OwningSection->GetEndTime() < CurrentTime)
			{
				OwningSection->SetEndTime(CurrentTime);
			}
		}

		if (Curve->GetNumKeys() == 0)
		{
			Curve->SetDefaultValue(Value);
		}
		else
		{
			Curve->UpdateOrAddKey(CurrentTime, Value);
		}
		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}

void SIntegralCurveKeyEditor::OnValueCommitted(int32 Value, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
	{
		const FScopedTransaction Transaction( LOCTEXT("SetIntegralKey", "Set Integral Key Value") );

		OnValueChanged(Value);
	}
}

#undef LOCTEXT_NAMESPACE