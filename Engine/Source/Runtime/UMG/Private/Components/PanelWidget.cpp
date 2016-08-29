// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#if WITH_EDITOR
#include "MessageLog.h"
#include "UObjectToken.h"
#endif

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UPanelWidget

UPanelWidget::UPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bCanHaveMultipleChildren(true)
{
}

void UPanelWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	if ( bReleaseChildren )
	{
		for ( int32 SlotIndex = 0; SlotIndex < Slots.Num(); SlotIndex++ )
		{
			if ( Slots[SlotIndex]->Content != nullptr )
			{
				Slots[SlotIndex]->ReleaseSlateResources(bReleaseChildren);
			}
		}
	}
}

int32 UPanelWidget::GetChildrenCount() const
{
	return Slots.Num();
}

UWidget* UPanelWidget::GetChildAt(int32 Index) const
{
	if ( Index < 0 || Index >= Slots.Num() )
	{
		return nullptr;
	}

	return Slots[Index]->Content;
}

int32 UPanelWidget::GetChildIndex(UWidget* Content) const
{
	const int32 ChildCount = GetChildrenCount();
	for ( int32 ChildIndex = 0; ChildIndex < ChildCount; ChildIndex++ )
	{
		if ( GetChildAt(ChildIndex) == Content )
		{
			return ChildIndex;
		}
	}

	return -1;
}

bool UPanelWidget::HasChild(UWidget* Content) const
{
	if ( !Content )
	{
		return false;
	}
	return ( Content->GetParent() == this );
}

bool UPanelWidget::RemoveChildAt(int32 Index)
{
	if ( Index < 0 || Index >= Slots.Num() )
	{
		return false;
	}

	UPanelSlot* PanelSlot = Slots[Index];
	if ( PanelSlot->Content )
	{
		PanelSlot->Content->Slot = nullptr;
	}

	Slots.RemoveAt(Index);

	OnSlotRemoved(PanelSlot);

	const bool bReleaseChildren = true;
	PanelSlot->ReleaseSlateResources(bReleaseChildren);

	PanelSlot->Parent = nullptr;
	PanelSlot->Content = nullptr;

	return true;
}

UPanelSlot* UPanelWidget::AddChild(UWidget* Content)
{
	if ( Content == nullptr )
	{
		return nullptr;
	}

	if ( !bCanHaveMultipleChildren && GetChildrenCount() > 0 )
	{
		return nullptr;
	}

	Content->RemoveFromParent();

	UPanelSlot* PanelSlot = NewObject<UPanelSlot>(this, GetSlotClass());
	PanelSlot->SetFlags(RF_Transactional);
	PanelSlot->Content = Content;
	PanelSlot->Parent = this;

	if ( Content )
	{
		Content->Slot = PanelSlot;
	}

	Slots.Add(PanelSlot);

	OnSlotAdded(PanelSlot);

	return PanelSlot;
}

bool UPanelWidget::ReplaceChildAt(int32 Index, UWidget* Content)
{
	if ( Index < 0 || Index >= Slots.Num() )
	{
		return false;
	}

	UPanelSlot* PanelSlot = Slots[Index];
	PanelSlot->Content = Content;

	if ( Content )
	{
		Content->Slot = PanelSlot;
	}

	PanelSlot->SynchronizeProperties();

	return true;
}

#if WITH_EDITOR

bool UPanelWidget::ReplaceChild(UWidget* CurrentChild, UWidget* NewChild)
{
	int32 Index = GetChildIndex(CurrentChild);
	if ( Index != -1 )
	{
		return ReplaceChildAt(Index, NewChild);
	}

	return false;
}

UPanelSlot* UPanelWidget::InsertChildAt(int32 Index, UWidget* Content)
{
	UPanelSlot* NewSlot = AddChild(Content);
	ShiftChild(Index, Content);
	return NewSlot;
}

void UPanelWidget::ShiftChild(int32 Index, UWidget* Child)
{
	int32 CurrentIndex = GetChildIndex(Child);
	Slots.RemoveAt(CurrentIndex);
	Slots.Insert(Child->Slot, FMath::Clamp(Index, 0, Slots.Num()));
}

void UPanelWidget::SetDesignerFlags(EWidgetDesignFlags::Type NewFlags)
{
	Super::SetDesignerFlags(NewFlags);

	// Also mark all children as design time widgets.
	int32 Children = GetChildrenCount();
	for ( int32 SlotIndex = 0; SlotIndex < Children; SlotIndex++ )
	{
		if ( Slots[SlotIndex]->Content != nullptr )
		{
			Slots[SlotIndex]->Content->SetDesignerFlags(NewFlags);
		}
	}
}

#endif

bool UPanelWidget::RemoveChild(UWidget* Content)
{
	int32 ChildIndex = GetChildIndex(Content);
	if ( ChildIndex != -1 )
	{
		return RemoveChildAt(ChildIndex);
	}

	return false;
}

bool UPanelWidget::HasAnyChildren() const
{
	return GetChildrenCount() > 0;
}

void UPanelWidget::ClearChildren()
{
	int32 Children = GetChildrenCount();
	for ( int32 ChildIndex = 0; ChildIndex < Children; ChildIndex++ )
	{
		RemoveChildAt(0);
	}
}

void UPanelWidget::PostLoad()
{
	Super::PostLoad();

	for ( int32 SlotIndex = 0; SlotIndex < Slots.Num(); SlotIndex++ )
	{
		// Remove any slots where their content is null, we don't support content-less slots.
		if (!Slots[SlotIndex] || Slots[SlotIndex]->Content == nullptr)
		{
			Slots.RemoveAt(SlotIndex);
			SlotIndex--;
		}
	}
}

const TArray<UPanelSlot*>& UPanelWidget::GetSlots() const
{
	return Slots;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
