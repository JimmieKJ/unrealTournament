// Copyright 1998t-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "MovieSceneStringSection.h"
#include "SStringCurveKeyEditor.h"

#define LOCTEXT_NAMESPACE "StringCurveKeyEditor"

void SStringCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	OnValueChangedEvent = InArgs._OnValueChanged;
	IntermediateValue = InArgs._IntermediateValue;
	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
	ChildSlot
	[
		SNew(SEditableText)
		.SelectAllTextWhenFocused(true)
		.Text(this, &SStringCurveKeyEditor::GetText)
		.OnTextCommitted(this, &SStringCurveKeyEditor::OnTextCommitted)
 	];
}

FText SStringCurveKeyEditor::GetText() const
{
	if ( IntermediateValue.IsSet() && IntermediateValue.Get().IsSet() )
	{
		return FText::FromString(IntermediateValue.Get().GetValue());
	}

	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());

	UMovieSceneStringSection* StringSection = Cast<UMovieSceneStringSection>(OwningSection);
	FString CurrentValue = StringSection ? StringSection->Eval(CurrentTime, Curve->GetDefaultValue()) : Curve->Eval(CurrentTime, Curve->GetDefaultValue());
	
	return FText::FromString(CurrentValue);
}

void SStringCurveKeyEditor::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	FScopedTransaction Transaction(LOCTEXT("SetStringKey", "Set String Key Value"));
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
			Curve->SetDefaultValue(InText.ToString());
		}
		else
		{
			Curve->UpdateOrAddKey(CurrentTime, InText.ToString());
		}

		OnValueChangedEvent.ExecuteIfBound(InText.ToString());

		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}

#undef LOCTEXT_NAMESPACE