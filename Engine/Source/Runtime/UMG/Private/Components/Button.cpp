// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UButton

UButton::UButton(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SButton::FArguments ButtonDefaults;
	WidgetStyle = *ButtonDefaults._ButtonStyle;

	ColorAndOpacity = FLinearColor::White;
	BackgroundColor = FLinearColor::White;

	ClickMethod = EButtonClickMethod::DownAndUp;
	TouchMethod = EButtonTouchMethod::DownAndUp;

	IsFocusable = true;
}

void UButton::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyButton.Reset();
}

TSharedRef<SWidget> UButton::RebuildWidget()
{
	MyButton = SNew(SButton)
		.OnClicked(BIND_UOBJECT_DELEGATE(FOnClicked, SlateHandleClicked))
		.OnPressed(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandlePressed))
		.OnReleased(BIND_UOBJECT_DELEGATE(FSimpleDelegate, SlateHandleReleased))
		.ButtonStyle(&WidgetStyle)
		.ClickMethod(ClickMethod)
		.TouchMethod(TouchMethod)
		.IsFocusable(IsFocusable)
		;

	if ( GetChildrenCount() > 0 )
	{
		Cast<UButtonSlot>(GetContentSlot())->BuildSlot(MyButton.ToSharedRef());
	}
	
	return MyButton.ToSharedRef();
}

void UButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	MyButton->SetColorAndOpacity( ColorAndOpacity );
	MyButton->SetBorderBackgroundColor( BackgroundColor );
}

UClass* UButton::GetSlotClass() const
{
	return UButtonSlot::StaticClass();
}

void UButton::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live slot if it already exists
	if ( MyButton.IsValid() )
	{
		Cast<UButtonSlot>(Slot)->BuildSlot(MyButton.ToSharedRef());
	}
}

void UButton::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyButton.IsValid() )
	{
		MyButton->SetContent(SNullWidget::NullWidget);
	}
}

void UButton::SetColorAndOpacity(FLinearColor Color)
{
	ColorAndOpacity = Color;
	if ( MyButton.IsValid() )
	{
		MyButton->SetColorAndOpacity(Color);
	}
}

void UButton::SetBackgroundColor(FLinearColor Color)
{
	BackgroundColor = Color;
	if ( MyButton.IsValid() )
	{
		MyButton->SetBorderBackgroundColor(Color);
	}
}

bool UButton::IsPressed() const
{
	if ( MyButton.IsValid() )
	{
		return MyButton->IsPressed();
	}

	return false;
}

void UButton::PostLoad()
{
	Super::PostLoad();

	if ( GetChildrenCount() > 0 )
	{
		//TODO UMG Pre-Release Upgrade, now buttons have slots of their own.  Convert existing slot to new slot.
		if ( UPanelSlot* PanelSlot = GetContentSlot() )
		{
			UButtonSlot* ButtonSlot = Cast<UButtonSlot>(PanelSlot);
			if ( ButtonSlot == NULL )
			{
				ButtonSlot = NewObject<UButtonSlot>(this);
				ButtonSlot->Content = GetContentSlot()->Content;
				ButtonSlot->Content->Slot = ButtonSlot;
				Slots[0] = ButtonSlot;
			}
		}
	}

	if( GetLinkerUE4Version() < VER_UE4_DEPRECATE_UMG_STYLE_ASSETS && Style_DEPRECATED != nullptr )
	{
		const FButtonStyle* StylePtr = Style_DEPRECATED->GetStyle<FButtonStyle>();
		if(StylePtr != nullptr)
		{
			WidgetStyle = *StylePtr;
		}

		Style_DEPRECATED = nullptr;
	}
}

FReply UButton::SlateHandleClicked()
{
	OnClicked.Broadcast();

	return FReply::Handled();
}

void UButton::SlateHandlePressed()
{
	OnPressed.Broadcast();
}

void UButton::SlateHandleReleased()
{
	OnReleased.Broadcast();
}

#if WITH_EDITOR

const FSlateBrush* UButton::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.Button");
}

const FText UButton::GetPaletteCategory()
{
	return LOCTEXT("Common", "Common");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
