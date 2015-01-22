// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MessageLogPrivatePCH.h"
#include "MessageFilter.h"

FReply FMessageFilter::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	RefreshCallback.Broadcast();
	return FReply::Handled();
}

ECheckBoxState FMessageFilter::OnGetDisplayCheckState() const
{
	return Display ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FMessageFilter::OnDisplayCheckStateChanged(ECheckBoxState InNewState)
{
	Display = InNewState == ECheckBoxState::Checked;
	RefreshCallback.Broadcast();
}
