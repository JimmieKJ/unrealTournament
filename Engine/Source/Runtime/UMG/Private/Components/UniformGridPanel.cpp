// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UUniformGridPanel

UUniformGridPanel::UUniformGridPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	SUniformGridPanel::FArguments Defaults;
	Visiblity_DEPRECATED = Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());
}

void UUniformGridPanel::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyUniformGridPanel.Reset();
}

UClass* UUniformGridPanel::GetSlotClass() const
{
	return UUniformGridSlot::StaticClass();
}

void UUniformGridPanel::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live canvas if it already exists
	if ( MyUniformGridPanel.IsValid() )
	{
		Cast<UUniformGridSlot>(Slot)->BuildSlot(MyUniformGridPanel.ToSharedRef());
	}
}

void UUniformGridPanel::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyUniformGridPanel.IsValid() )
	{
		TSharedPtr<SWidget> Widget = Slot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyUniformGridPanel->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UUniformGridPanel::RebuildWidget()
{
	MyUniformGridPanel = SNew(SUniformGridPanel);

	for ( UPanelSlot* Slot : Slots )
	{
		if ( UUniformGridSlot* TypedSlot = Cast<UUniformGridSlot>(Slot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyUniformGridPanel.ToSharedRef());
		}
	}

	return BuildDesignTimeWidget( MyUniformGridPanel.ToSharedRef() );
}

UUniformGridSlot* UUniformGridPanel::AddChildToUniformGrid(UWidget* Content)
{
	return Cast<UUniformGridSlot>(Super::AddChild(Content));
}

void UUniformGridPanel::SetSlotPadding(FMargin InSlotPadding)
{
	SlotPadding = InSlotPadding;
	if ( MyUniformGridPanel.IsValid() )
	{
		MyUniformGridPanel->SetSlotPadding(InSlotPadding);
	}
}

void UUniformGridPanel::SetMinDesiredSlotWidth(float InMinDesiredSlotWidth)
{
	MinDesiredSlotWidth = InMinDesiredSlotWidth;
	if ( MyUniformGridPanel.IsValid() )
	{
		MyUniformGridPanel->SetMinDesiredSlotWidth(InMinDesiredSlotWidth);
	}
}

void UUniformGridPanel::SetMinDesiredSlotHeight(float InMinDesiredSlotHeight)
{
	MinDesiredSlotHeight = InMinDesiredSlotHeight;
	if ( MyUniformGridPanel.IsValid() )
	{
		MyUniformGridPanel->SetMinDesiredSlotHeight(InMinDesiredSlotHeight);
	}
}

void UUniformGridPanel::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyUniformGridPanel->SetSlotPadding(SlotPadding);
	MyUniformGridPanel->SetMinDesiredSlotWidth(MinDesiredSlotWidth);
	MyUniformGridPanel->SetMinDesiredSlotHeight(MinDesiredSlotHeight);
}

#if WITH_EDITOR

const FSlateBrush* UUniformGridPanel::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.UniformGrid");
}

const FText UUniformGridPanel::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
