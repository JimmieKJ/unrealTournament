// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneBoolSection.h"
#include "SBoolCurveKeyEditor.h"

#define LOCTEXT_NAMESPACE "BoolCurveKeyEditor"

void SBoolCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	OnValueChangedEvent = InArgs._OnValueChanged;
	IntermediateValue = InArgs._IntermediateValue;
	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
	ChildSlot
	[
		SNew(SCheckBox)
		.IsChecked(this, &SBoolCurveKeyEditor::IsChecked)
		.OnCheckStateChanged(this, &SBoolCurveKeyEditor::OnCheckStateChanged)
	];
}

ECheckBoxState SBoolCurveKeyEditor::IsChecked() const
{
	if ( IntermediateValue.IsSet() && IntermediateValue.Get().IsSet() )
	{
		return IntermediateValue.Get().GetValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());

	UMovieSceneBoolSection* BoolSection = Cast<UMovieSceneBoolSection>(OwningSection);
	bool bChecked = BoolSection ? BoolSection->Eval(CurrentTime) : Curve->Evaluate(CurrentTime) != 0;
	
	return bChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SBoolCurveKeyEditor::OnCheckStateChanged(ECheckBoxState NewCheckboxState)
{
	FScopedTransaction Transaction(LOCTEXT("SetBoolKey", "Set Bool Key Value"));
	OwningSection->SetFlags(RF_Transactional);
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
		
		int32 NewValue = NewCheckboxState == ECheckBoxState::Checked ? 1 : 0;
		if (Curve->GetNumKeys() == 0)
		{
			Curve->SetDefaultValue(NewValue);
		}
		else
		{
			Curve->UpdateOrAddKey(CurrentTime, NewValue);
		}

		OnValueChangedEvent.ExecuteIfBound(NewCheckboxState == ECheckBoxState::Checked);

		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}

#undef LOCTEXT_NAMESPACE