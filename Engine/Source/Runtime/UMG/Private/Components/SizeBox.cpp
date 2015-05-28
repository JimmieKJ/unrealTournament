// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "Slate/SlateBrushAsset.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// USizeBox

USizeBox::USizeBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;
	Visibility = ESlateVisibility::SelfHitTestInvisible;
}

void USizeBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MySizeBox.Reset();
}

TSharedRef<SWidget> USizeBox::RebuildWidget()
{
	MySizeBox = SNew(SBox);
	
	if ( GetChildrenCount() > 0 )
	{
		Cast<USizeBoxSlot>(GetContentSlot())->BuildSlot(MySizeBox.ToSharedRef());
	}

	return BuildDesignTimeWidget( MySizeBox.ToSharedRef() );
}

void USizeBox::SetWidthOverride(float InWidthOverride)
{
	bOverride_WidthOverride = true;
	WidthOverride = InWidthOverride;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetWidthOverride(InWidthOverride);
	}
}

void USizeBox::ClearWidthOverride()
{
	bOverride_WidthOverride = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetWidthOverride(FOptionalSize());
	}
}

void USizeBox::SetHeightOverride(float InHeightOverride)
{
	bOverride_HeightOverride = true;
	HeightOverride = InHeightOverride;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetHeightOverride(InHeightOverride);
	}
}

void USizeBox::ClearHeightOverride()
{
	bOverride_HeightOverride = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetHeightOverride(FOptionalSize());
	}
}

void USizeBox::SetMinDesiredWidth(float InMinDesiredWidth)
{
	bOverride_MinDesiredWidth = true;
	MinDesiredWidth = InMinDesiredWidth;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMinDesiredWidth(InMinDesiredWidth);
	}
}

void USizeBox::ClearMinDesiredWidth()
{
	bOverride_MinDesiredWidth = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMinDesiredWidth(FOptionalSize());
	}
}

void USizeBox::SetMinDesiredHeight(float InMinDesiredHeight)
{
	bOverride_MinDesiredHeight = true;
	MinDesiredHeight = InMinDesiredHeight;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMinDesiredHeight(InMinDesiredHeight);
	}
}

void USizeBox::ClearMinDesiredHeight()
{
	bOverride_MinDesiredHeight = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMinDesiredHeight(FOptionalSize());
	}
}

void USizeBox::SetMaxDesiredWidth(float InMaxDesiredWidth)
{
	bOverride_MaxDesiredWidth = true;
	MaxDesiredWidth = InMaxDesiredWidth;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxDesiredWidth(InMaxDesiredWidth);
	}
}

void USizeBox::ClearMaxDesiredWidth()
{
	bOverride_MaxDesiredWidth = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxDesiredWidth(FOptionalSize());
	}
}

void USizeBox::SetMaxDesiredHeight(float InMaxDesiredHeight)
{
	bOverride_MaxDesiredHeight = true;
	MaxDesiredHeight = InMaxDesiredHeight;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxDesiredHeight(InMaxDesiredHeight);
	}
}

void USizeBox::ClearMaxDesiredHeight()
{
	bOverride_MaxDesiredHeight = false;
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetMaxDesiredHeight(FOptionalSize());
	}
}

void USizeBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if ( bOverride_WidthOverride )
	{
		SetWidthOverride(WidthOverride);
	}
	else
	{
		ClearWidthOverride();
	}

	if ( bOverride_HeightOverride )
	{
		SetHeightOverride(HeightOverride);
	}
	else
	{
		ClearHeightOverride();
	}

	if ( bOverride_MinDesiredWidth )
	{
		SetMinDesiredWidth(MinDesiredWidth);
	}
	else
	{
		ClearMinDesiredWidth();
	}

	if ( bOverride_MinDesiredHeight )
	{
		SetMinDesiredHeight(MinDesiredHeight);
	}
	else
	{
		ClearMinDesiredHeight();
	}

	if ( bOverride_MaxDesiredWidth )
	{
		SetMaxDesiredWidth(MaxDesiredWidth);
	}
	else
	{
		ClearMaxDesiredWidth();
	}

	if ( bOverride_MaxDesiredHeight )
	{
		SetMaxDesiredHeight(MaxDesiredHeight);
	}
	else
	{
		ClearMaxDesiredHeight();
	}
}

UClass* USizeBox::GetSlotClass() const
{
	return USizeBoxSlot::StaticClass();
}

void USizeBox::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live slot if it already exists
	if ( MySizeBox.IsValid() )
	{
		Cast<USizeBoxSlot>(Slot)->BuildSlot(MySizeBox.ToSharedRef());
	}
}

void USizeBox::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( MySizeBox.IsValid() )
	{
		MySizeBox->SetContent(SNullWidget::NullWidget);
	}
}

#if WITH_EDITOR

const FSlateBrush* USizeBox::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.SizeBox");
}

const FText USizeBox::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
