// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UNamedSlot

UNamedSlot::UNamedSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
	Visibility = ESlateVisibility::SelfHitTestInvisible;
}

void UNamedSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyBox.Reset();
}

TSharedRef<SWidget> UNamedSlot::RebuildWidget()
{
	MyBox = SNew(SBox);

	if ( IsDesignTime() )
	{
		MyBox->SetContent(
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(GetFName()))
			]
		);
	}

	// Add any existing content to the new slate box
	if ( GetChildrenCount() > 0 )
	{
		UPanelSlot* ContentSlot = GetContentSlot();
		if ( ContentSlot->Content )
		{
			MyBox->SetContent(ContentSlot->Content->TakeWidget());
		}
	}

	return MyBox.ToSharedRef();
}

void UNamedSlot::SynchronizeProperties()
{
	Super::SynchronizeProperties();
}

void UNamedSlot::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live slot if it already exists
	if ( MyBox.IsValid() && Slot->Content )
	{
		MyBox->SetContent(Slot->Content->TakeWidget());
	}
}

void UNamedSlot::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyBox.IsValid() )
	{
		MyBox->SetContent(SNullWidget::NullWidget);

		if ( IsDesignTime() )
		{
			MyBox->SetContent(
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromName(GetFName()))
				]
			);
		}
	}
}

#if WITH_EDITOR

const FSlateBrush* UNamedSlot::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.NamedSlot");
}

const FText UNamedSlot::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
