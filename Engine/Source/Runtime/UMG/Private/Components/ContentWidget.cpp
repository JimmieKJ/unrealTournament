// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#if WITH_EDITOR
#include "MessageLog.h"
#include "UObjectToken.h"
#endif

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UContentWidget

UContentWidget::UContentWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanHaveMultipleChildren = false;
}

UPanelSlot* UContentWidget::GetContentSlot() const
{
	return Slots.Num() > 0 ? Slots[0] : nullptr;
}

UPanelSlot* UContentWidget::SetContent(UWidget* Content)
{
	ClearChildren();
	return AddChild(Content);
}

UClass* UContentWidget::GetSlotClass() const
{
	return UPanelSlot::StaticClass();
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
