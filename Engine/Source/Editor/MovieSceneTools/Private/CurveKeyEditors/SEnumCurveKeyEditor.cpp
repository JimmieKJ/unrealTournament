// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "SEnumCurveKeyEditor.h"

#define LOCTEXT_NAMESPACE "EnumCurveKeyEditor"

void SEnumCurveKeyEditor::Construct(const FArguments& InArgs)
{
	Sequencer = InArgs._Sequencer;
	OwningSection = InArgs._OwningSection;
	Curve = InArgs._Curve;
	Enum = InArgs._Enum;
	IntermediateValue = InArgs._IntermediateValue;

	for (int32 i = 0; i < InArgs._Enum->NumEnums() - 1; i++)
	{
		if (Enum->HasMetaData( TEXT("Hidden"), i ) == false)
		{
			VisibleEnumNameIndices.Add(MakeShareable(new int32(i)));
		}
	}

	ChildSlot
	[
		SAssignNew(EnumValuesCombo, SComboBox<TSharedPtr<int32>>)
		.ButtonStyle(FEditorStyle::Get(), "FlatButton.Light")
		.OptionsSource(&VisibleEnumNameIndices)
		.OnGenerateWidget(this, &SEnumCurveKeyEditor::OnGenerateWidget)
		.OnSelectionChanged(this, &SEnumCurveKeyEditor::OnComboSelectionChanged)
		.OnComboBoxOpening(this, &SEnumCurveKeyEditor::OnComboMenuOpening)
		.ContentPadding(FMargin(2, 0))
		[
			SNew(STextBlock)
			.Font(FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
			.Text(this, &SEnumCurveKeyEditor::GetCurrentValue)
		]
	];
	bUpdatingSelectionInternally = false;
}

FText SEnumCurveKeyEditor::GetCurrentValue() const
{
	if ( IntermediateValue.IsSet() && IntermediateValue.Get().IsSet() )
	{
		int32 IntermediateNameIndex = Enum->GetIndexByValue( IntermediateValue.Get().GetValue() );
		return Enum->GetDisplayNameText( IntermediateNameIndex );
	}

	float CurrentTime = Sequencer->GetCurrentLocalTime(*Sequencer->GetFocusedMovieSceneSequence());
	int32 CurrentNameIndex = Enum->GetIndexByValue(Curve->Evaluate(CurrentTime));
	return Enum->GetDisplayNameText(CurrentNameIndex);
}

TSharedRef<SWidget> SEnumCurveKeyEditor::OnGenerateWidget(TSharedPtr<int32> InItem)
{
	return SNew(STextBlock)
		.Text(Enum->GetDisplayNameText(*InItem));
}

void SEnumCurveKeyEditor::OnComboSelectionChanged(TSharedPtr<int32> InSelectedItem, ESelectInfo::Type SelectInfo)
{
	if (bUpdatingSelectionInternally == false)
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

			int32 SelectedValue = Enum->GetValueByIndex(*InSelectedItem);
			if (Curve->GetNumKeys() == 0)
			{
				Curve->SetDefaultValue(SelectedValue);
			}
			else
			{
				Curve->UpdateOrAddKey(CurrentTime, SelectedValue);
			}
			Sequencer->UpdateRuntimeInstances();
		}
	}
}

void SEnumCurveKeyEditor::OnComboMenuOpening()
{
	float CurrentTime = Sequencer->GetCurrentLocalTime( *Sequencer->GetFocusedMovieSceneSequence() );
	int32 CurrentNameIndex = Enum->GetIndexByValue( Curve->Evaluate( CurrentTime ) );
	TSharedPtr<int32> FoundNameIndexItem;
	for ( int32 i = 0; i < VisibleEnumNameIndices.Num(); i++ )
	{
		if ( *VisibleEnumNameIndices[i] == CurrentNameIndex )
		{
			FoundNameIndexItem = VisibleEnumNameIndices[i];
			break;
		}
	}
	if ( FoundNameIndexItem.IsValid() )
	{
		bUpdatingSelectionInternally = true;
		EnumValuesCombo->SetSelectedItem(FoundNameIndexItem);
		bUpdatingSelectionInternally = false;
	}
}

#undef LOCTEXT_NAMESPACE