// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "TAttributeProperty.h"
#include "Slate.h"

ESlateCheckBoxState::Type TAttributePropertyBool::GetAsCheckBox() const
{
	return Get() ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked;
}

void TAttributePropertyBool::SetFromCheckBox(ESlateCheckBoxState::Type CheckedState)
{
	if (CheckedState == ESlateCheckBoxState::Checked)
	{
		Set(true);
	}
	else if (CheckedState == ESlateCheckBoxState::Unchecked)
	{
		Set(false);
	}
}