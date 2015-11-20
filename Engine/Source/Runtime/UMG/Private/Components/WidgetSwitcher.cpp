// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UWidgetSwitcher

UWidgetSwitcher::UWidgetSwitcher(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;

	SWidgetSwitcher::FArguments Defaults;
	Visiblity_DEPRECATED = Visibility = Visiblity_DEPRECATED = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());
}

void UWidgetSwitcher::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyWidgetSwitcher.Reset();
}

int32 UWidgetSwitcher::GetNumWidgets() const
{
	if ( MyWidgetSwitcher.IsValid() )
	{
		return MyWidgetSwitcher->GetNumWidgets();
	}

	return Slots.Num();
}

int32 UWidgetSwitcher::GetActiveWidgetIndex() const
{
	if ( MyWidgetSwitcher.IsValid() )
	{
		return MyWidgetSwitcher->GetActiveWidgetIndex();
	}

	return ActiveWidgetIndex;
}

void UWidgetSwitcher::SetActiveWidgetIndex(int32 Index)
{
	ActiveWidgetIndex = Index;
	if ( MyWidgetSwitcher.IsValid() )
	{
		// Ensure the index is clamped to a valid range.
		int32 SafeIndex = FMath::Clamp(ActiveWidgetIndex, 0, FMath::Max(0, Slots.Num() - 1));
		MyWidgetSwitcher->SetActiveWidgetIndex(SafeIndex);
	}
}

void UWidgetSwitcher::SetActiveWidget(UWidget* Widget)
{
	ActiveWidgetIndex = GetChildIndex(Widget);
	if ( MyWidgetSwitcher.IsValid() )
	{
		// Ensure the index is clamped to a valid range.
		int32 SafeIndex = FMath::Clamp(ActiveWidgetIndex, 0, FMath::Max(0, Slots.Num() - 1));
		MyWidgetSwitcher->SetActiveWidgetIndex(SafeIndex);
	}
}

UWidget* UWidgetSwitcher::GetWidgetAtIndex( int32 Index ) const
{
	if ( Slots.IsValidIndex( Index ) )
	{
		return Slots[ Index ]->Content;
	}

	return nullptr;
}

UClass* UWidgetSwitcher::GetSlotClass() const
{
	return UWidgetSwitcherSlot::StaticClass();
}

void UWidgetSwitcher::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live canvas if it already exists
	if ( MyWidgetSwitcher.IsValid() )
	{
		Cast<UWidgetSwitcherSlot>(Slot)->BuildSlot(MyWidgetSwitcher.ToSharedRef());
	}
}

void UWidgetSwitcher::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyWidgetSwitcher.IsValid() )
	{
		TSharedPtr<SWidget> Widget = Slot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyWidgetSwitcher->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UWidgetSwitcher::RebuildWidget()
{
	MyWidgetSwitcher = SNew(SWidgetSwitcher);

	for ( UPanelSlot* Slot : Slots )
	{
		if ( UWidgetSwitcherSlot* TypedSlot = Cast<UWidgetSwitcherSlot>(Slot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyWidgetSwitcher.ToSharedRef());
		}
	}

	return BuildDesignTimeWidget( MyWidgetSwitcher.ToSharedRef() );
}

void UWidgetSwitcher::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	SetActiveWidgetIndex(ActiveWidgetIndex);
}

#if WITH_EDITOR

const FSlateBrush* UWidgetSwitcher::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.WidgetSwitcher");
}

const FText UWidgetSwitcher::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

void UWidgetSwitcher::OnDescendantSelected(UWidget* DescendantWidget)
{
	// Temporarily sets the active child to the selected child to make
	// dragging and dropping easier in the editor.
	UWidget* SelectedChild = UWidget::FindChildContainingDescendant(this, DescendantWidget);
	if ( SelectedChild )
	{
		int32 OverrideIndex = GetChildIndex(SelectedChild);
		if ( OverrideIndex != -1 && MyWidgetSwitcher.IsValid() )
		{
			MyWidgetSwitcher->SetActiveWidgetIndex(OverrideIndex);
		}
	}
}

void UWidgetSwitcher::OnDescendantDeselected(UWidget* DescendantWidget)
{
	SetActiveWidgetIndex(ActiveWidgetIndex);
}

void UWidgetSwitcher::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	ActiveWidgetIndex = FMath::Clamp(ActiveWidgetIndex, 0, FMath::Max(0, Slots.Num() - 1));

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
