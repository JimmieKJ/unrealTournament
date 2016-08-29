// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneToolHelpers.h"
#include "SEnumCurveKeyEditor.h"


#define LOCTEXT_NAMESPACE "EnumCurveKeyEditor"


void SEnumCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	OnValueChangedEvent = InArgs._OnValueChanged;

	ChildSlot
	[
		MovieSceneToolHelpers::MakeEnumComboBox(
			InArgs._Enum,
			TAttribute<int32>::Create(TAttribute<int32>::FGetter::CreateSP(this, &SEnumCurveKeyEditor::OnGetCurrentValue)),
			FOnEnumSelectionChanged::CreateSP(this, &SEnumCurveKeyEditor::OnComboSelectionChanged),
			InArgs._IntermediateValue
		)
	];
}


int32 SEnumCurveKeyEditor::OnGetCurrentValue() const
{
	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
	return Curve->Evaluate(CurrentTime);
}


void SEnumCurveKeyEditor::OnComboSelectionChanged(int32 InSelectedItem, ESelectInfo::Type SelectInfo)
{
	FScopedTransaction Transaction(LOCTEXT("SetEnumKey", "Set Enum Key Value"));
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

		if (Curve->GetNumKeys() == 0)
		{
			Curve->SetDefaultValue(InSelectedItem);
		}
		else
		{
			Curve->UpdateOrAddKey(CurrentTime, InSelectedItem);
		}

		OnValueChangedEvent.ExecuteIfBound(InSelectedItem);

		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}


#undef LOCTEXT_NAMESPACE
