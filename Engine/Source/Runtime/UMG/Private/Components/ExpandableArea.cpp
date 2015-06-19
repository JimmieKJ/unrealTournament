// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "SExpandableArea.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UExpandableArea

static const FName HeaderName(TEXT("Header"));
static const FName BodyName(TEXT("Body"));

UExpandableArea::UExpandableArea(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsExpanded(false)
{
	bIsVariable = false;
}

bool UExpandableArea::GetIsExpanded() const
{
	if ( MyExpandableArea.IsValid() )
	{
		return MyExpandableArea->IsExpanded();
	}

	return bIsExpanded;
}

void UExpandableArea::SetIsExpanded(bool IsExpanded)
{
	bIsExpanded = IsExpanded;
	if ( MyExpandableArea.IsValid() )
	{
		MyExpandableArea->SetExpanded(IsExpanded);
	}
}

void UExpandableArea::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyExpandableArea.Reset();
}

void UExpandableArea::GetSlotNames(TArray<FName>& SlotNames) const
{
	SlotNames.Add(HeaderName);
	SlotNames.Add(BodyName);
}

UWidget* UExpandableArea::GetContentForSlot(FName SlotName) const
{
	if ( SlotName == HeaderName )
	{
		return HeaderContent;
	}
	else if ( SlotName == BodyName )
	{
		return BodyContent;
	}

	return nullptr;
}

void UExpandableArea::SetContentForSlot(FName SlotName, UWidget* Content)
{
	if ( SlotName == HeaderName )
	{
		HeaderContent = Content;
	}
	else if ( SlotName == BodyName )
	{
		BodyContent = Content;
	}
}

TSharedRef<SWidget> UExpandableArea::RebuildWidget()
{
	TSharedRef<SWidget> HeaderWidget = HeaderContent ? HeaderContent->TakeWidget() : SNullWidget::NullWidget;
	TSharedRef<SWidget> BodyWidget = BodyContent ? BodyContent->TakeWidget() : SNullWidget::NullWidget;

	MyExpandableArea = SNew(SExpandableArea)
		.MaxHeight(MaxHeight)
		.Padding(AreaPadding)
		.OnAreaExpansionChanged(BIND_UOBJECT_DELEGATE(FOnBooleanValueChanged, SlateExpansionChanged))
		.HeaderContent()
		[
			HeaderWidget
		]
		.BodyContent()
		[
			BodyWidget
		];

	return MyExpandableArea.ToSharedRef();
}

void UExpandableArea::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyExpandableArea->SetExpanded(bIsExpanded);
}

void UExpandableArea::SlateExpansionChanged(bool NewState)
{
	bIsExpanded = NewState;

	if ( OnExpansionChanged.IsBound() )
	{
		OnExpansionChanged.Broadcast(NewState);
	}
}

#if WITH_EDITOR

const FSlateBrush* UExpandableArea::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.Spacer");
}

const FText UExpandableArea::GetPaletteCategory()
{
	return LOCTEXT("Extra", "Extra");
}

void UExpandableArea::OnDescendantSelected(UWidget* DescendantWidget)
{
	// Temporarily sets the active child to the selected child to make
	// dragging and dropping easier in the editor.
	UWidget* SelectedChild = UWidget::FindChildContainingDescendant(BodyContent, DescendantWidget);
	if ( SelectedChild )
	{
		MyExpandableArea->SetExpanded(true);
	}
}

void UExpandableArea::OnDescendantDeselected(UWidget* DescendantWidget)
{
	if ( MyExpandableArea.IsValid() )
	{
		MyExpandableArea->SetExpanded(bIsExpanded);
	}
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
