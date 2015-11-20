// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UVerticalBox

UVerticalBox::UVerticalBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	SVerticalBox::FArguments Defaults;
	Visiblity_DEPRECATED = Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());
}

void UVerticalBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyVerticalBox.Reset();
}

UClass* UVerticalBox::GetSlotClass() const
{
	return UVerticalBoxSlot::StaticClass();
}

void UVerticalBox::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live canvas if it already exists
	if ( MyVerticalBox.IsValid() )
	{
		Cast<UVerticalBoxSlot>(Slot)->BuildSlot(MyVerticalBox.ToSharedRef());
	}
}

void UVerticalBox::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyVerticalBox.IsValid() )
	{
		TSharedPtr<SWidget> Widget = Slot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyVerticalBox->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

UVerticalBoxSlot* UVerticalBox::AddChildToVerticalBox(UWidget* Content)
{
	return Cast<UVerticalBoxSlot>(Super::AddChild(Content));
}

TSharedRef<SWidget> UVerticalBox::RebuildWidget()
{
	MyVerticalBox = SNew(SVerticalBox);

	for ( UPanelSlot* Slot : Slots )
	{
		if ( UVerticalBoxSlot* TypedSlot = Cast<UVerticalBoxSlot>(Slot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyVerticalBox.ToSharedRef());
		}
	}

	return BuildDesignTimeWidget( MyVerticalBox.ToSharedRef() );
}

#if WITH_EDITOR

const FSlateBrush* UVerticalBox::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.VerticalBox");
}

const FText UVerticalBox::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE