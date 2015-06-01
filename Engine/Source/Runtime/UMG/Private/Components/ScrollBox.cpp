// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UScrollBox

UScrollBox::UScrollBox(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = false;

	Orientation = Orient_Vertical;

	SScrollBox::FArguments Defaults;
	Visiblity_DEPRECATED = Visibility = UWidget::ConvertRuntimeToSerializedVisibility(Defaults._Visibility.Get());

	WidgetStyle = *Defaults._Style;
	WidgetBarStyle = *Defaults._ScrollBarStyle;

	AlwaysShowScrollbar = false;
	ScrollbarThickness = FVector2D(5, 5);
	ScrollBarVisibility = ESlateVisibility::Visible;

	ConsumeMouseWheel = EConsumeMouseWheel::WhenScrollingPossible;
}

void UScrollBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyScrollBox.Reset();
}

UClass* UScrollBox::GetSlotClass() const
{
	return UScrollBoxSlot::StaticClass();
}

void UScrollBox::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live canvas if it already exists
	if ( MyScrollBox.IsValid() )
	{
		Cast<UScrollBoxSlot>(Slot)->BuildSlot(MyScrollBox.ToSharedRef());
	}
}

void UScrollBox::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyScrollBox.IsValid() )
	{
		TSharedPtr<SWidget> Widget = Slot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyScrollBox->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UScrollBox::RebuildWidget()
{
	MyScrollBox = SNew(SScrollBox)
		.Style(&WidgetStyle)
		.ScrollBarStyle(&WidgetBarStyle)
		.Orientation(Orientation)
		.ConsumeMouseWheel(ConsumeMouseWheel);

	for ( UPanelSlot* Slot : Slots )
	{
		if ( UScrollBoxSlot* TypedSlot = Cast<UScrollBoxSlot>(Slot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyScrollBox.ToSharedRef());
		}
	}
	
	return BuildDesignTimeWidget( MyScrollBox.ToSharedRef() );
}

void UScrollBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyScrollBox->SetScrollOffset(DesiredScrollOffset);
	MyScrollBox->SetOrientation(Orientation);
	MyScrollBox->SetScrollBarVisibility(UWidget::ConvertSerializedVisibilityToRuntime(ScrollBarVisibility));
	MyScrollBox->SetScrollBarThickness(ScrollbarThickness);
	MyScrollBox->SetScrollBarAlwaysVisible(AlwaysShowScrollbar);
}

float UScrollBox::GetScrollOffset() const
{
	if ( MyScrollBox.IsValid() )
	{
		return MyScrollBox->GetScrollOffset();
	}

	return 0;
}

void UScrollBox::SetScrollOffset(float NewScrollOffset)
{
	DesiredScrollOffset = NewScrollOffset;

	if ( MyScrollBox.IsValid() )
	{
		MyScrollBox->SetScrollOffset(NewScrollOffset);
	}
}

void UScrollBox::ScrollToStart()
{
	if ( MyScrollBox.IsValid() )
	{
		MyScrollBox->ScrollToStart();
	}
}

void UScrollBox::ScrollToEnd()
{
	if ( MyScrollBox.IsValid() )
	{
		MyScrollBox->ScrollToEnd();
	}
}

void UScrollBox::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS )
	{
		if ( Style_DEPRECATED != nullptr )
		{
			const FScrollBoxStyle* StylePtr = Style_DEPRECATED->GetStyle<FScrollBoxStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetStyle = *StylePtr;
			}

			Style_DEPRECATED = nullptr;
		}

		if ( BarStyle_DEPRECATED != nullptr )
		{
			const FScrollBarStyle* StylePtr = BarStyle_DEPRECATED->GetStyle<FScrollBarStyle>();
			if ( StylePtr != nullptr )
			{
				WidgetBarStyle = *StylePtr;
			}

			BarStyle_DEPRECATED = nullptr;
		}
	}
}

#if WITH_EDITOR

const FSlateBrush* UScrollBox::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.ScrollBox");
}

const FText UScrollBox::GetPaletteCategory()
{
	return LOCTEXT("Panel", "Panel");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
