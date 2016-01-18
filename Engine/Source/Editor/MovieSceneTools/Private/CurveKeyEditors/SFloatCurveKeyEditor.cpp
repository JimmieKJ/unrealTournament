// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "SFloatCurveKeyEditor.h"

#define LOCTEXT_NAMESPACE "FloatCurveKeyEditor"

void SFloatCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	IntermediateValue = InArgs._IntermediateValue;
	ChildSlot
	[
		SNew(SSpinBox<float>)
		.Style(&FEditorStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
		.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
		.MinValue(TOptional<float>())
		.MaxValue(TOptional<float>())
		.MaxSliderValue(TOptional<float>())
		.MinSliderValue(TOptional<float>())
		.Delta(0.001f)
		.Value(this, &SFloatCurveKeyEditor::OnGetKeyValue)
		.OnValueChanged(this, &SFloatCurveKeyEditor::OnValueChanged)
		.OnValueCommitted(this, &SFloatCurveKeyEditor::OnValueCommitted)
		.OnBeginSliderMovement(this, &SFloatCurveKeyEditor::OnBeginSliderMovement)
		.OnEndSliderMovement(this, &SFloatCurveKeyEditor::OnEndSliderMovement)
		.ClearKeyboardFocusOnCommit(true)
	];
}

void SFloatCurveKeyEditor::OnBeginSliderMovement()
{
	GEditor->BeginTransaction(LOCTEXT("SetFloatKey", "Set Float Key Value"));
	OwningSection->SetFlags(RF_Transactional);
	OwningSection->TryModify();
}

void SFloatCurveKeyEditor::OnEndSliderMovement(float Value)
{
	if (GEditor->IsTransactionActive())
	{
		GEditor->EndTransaction();
	}
}

float SFloatCurveKeyEditor::OnGetKeyValue() const
{
	if ( IntermediateValue.IsSet() && IntermediateValue.Get().IsSet() )
	{
		return IntermediateValue.Get().GetValue();
	}

	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
	return Curve->Eval(CurrentTime);
}

void SFloatCurveKeyEditor::OnValueChanged(float Value)
{
	if (OwningSection->TryModify())
	{
		float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
		FKeyHandle CurrentKeyHandle = Curve->FindKey(CurrentTime);
		if (Curve->IsKeyHandleValid(CurrentKeyHandle))
		{
			Curve->SetKeyValue(CurrentKeyHandle, Value);
		}
		else
		{
			if (Curve->GetNumKeys() == 0)
			{
				Curve->SetDefaultValue(Value);
			}
			else
			{
				Curve->AddKey(CurrentTime, Value, false, CurrentKeyHandle);

				MovieSceneHelpers::SetKeyInterpolation(*Curve, CurrentKeyHandle, Sequencer->GetKeyInterpolation());
			}

			if (OwningSection->GetStartTime() > CurrentTime)
			{
				OwningSection->SetStartTime(CurrentTime);
			}
			if (OwningSection->GetEndTime() < CurrentTime)
			{
				OwningSection->SetEndTime(CurrentTime);
			}
		}
		Sequencer->UpdateRuntimeInstances();
	}
}

void SFloatCurveKeyEditor::OnValueCommitted(float Value, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		const FScopedTransaction Transaction( LOCTEXT("SetFloatKey", "Set Float Key Value") );

		OnValueChanged(Value);
	}
}

#undef LOCTEXT_NAMESPACE