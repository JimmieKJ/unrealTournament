// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "UnrealTournament.h"
#include "TAttributeProperty.h"
#include "SlateBasics.h"

ECheckBoxState TAttributePropertyBool::GetAsCheckBox() const
{
	return Get() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void TAttributePropertyBool::SetFromCheckBox(ECheckBoxState CheckedState)
{
	if (CheckedState == ECheckBoxState::Checked)
	{
		Set(true);
	}
	else if (CheckedState == ECheckBoxState::Unchecked)
	{
		Set(false);
	}
}