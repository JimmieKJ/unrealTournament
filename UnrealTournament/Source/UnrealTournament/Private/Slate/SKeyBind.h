// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Base/SUTDialogBase.h"
#include "../SUTStyle.h"
#include "../SUTUtils.h"


DECLARE_DELEGATE_TwoParams(FOnKeyBindingChanged, FKey,FKey);

class UNREALTOURNAMENT_API SKeyBind : public SButton
{
public:

	SLATE_BEGIN_ARGS(SKeyBind)
		: _ButtonStyle(&FCoreStyle::Get().GetWidgetStyle< FButtonStyle >("Button"))
		, _TextStyle(&FCoreStyle::Get().GetWidgetStyle< FTextBlockStyle >("NormalText"))
		, _HAlign(HAlign_Center)
		, _VAlign(VAlign_Center)
		, _ContentPadding(FMargin(4.0, 2.0))
		, _DesiredSizeScale(FVector2D(1, 1))
		, _ContentScale(FVector2D(1, 1))
		, _ButtonColorAndOpacity(FLinearColor::White)
		, _ForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
		, _Key(nullptr)
	{}
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)
		SLATE_STYLE_ARGUMENT(FTextBlockStyle, TextStyle)
		SLATE_ARGUMENT(EHorizontalAlignment, HAlign)
		SLATE_ARGUMENT(EVerticalAlignment, VAlign)
		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		SLATE_ATTRIBUTE(FVector2D, DesiredSizeScale)
		SLATE_ATTRIBUTE(FVector2D, ContentScale)
		SLATE_ATTRIBUTE(FSlateColor, ButtonColorAndOpacity)
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)
		SLATE_ARGUMENT(TSharedPtr<FKey>, Key)
		SLATE_ARGUMENT(FKey, DefaultKey)
		SLATE_EVENT(FOnKeyBindingChanged, OnKeyBindingChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SButton::Construct(SButton::FArguments()
			.ButtonStyle(InArgs._ButtonStyle)
			.TextStyle(InArgs._TextStyle)
			.HAlign(HAlign_Fill)
			.VAlign(InArgs._VAlign)
			.ContentPadding(InArgs._ContentPadding)
			.DesiredSizeScale(InArgs._DesiredSizeScale)
			.ContentScale(InArgs._ContentScale)
			.ButtonColorAndOpacity(InArgs._ButtonColorAndOpacity)
			.ForegroundColor(InArgs._ForegroundColor)
		);

		DefaultKey = InArgs._DefaultKey;
		OnKeyBindingChanged = InArgs._OnKeyBindingChanged;

		NonDefaultKeyColor = FLinearColor(0.0f, 0.0f, 0.24f, 1.0f);
#if !UE_SERVER
		ChildSlot
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().HAlign(HAlign_Center)
					[
						SAssignNew(KeyText, STextBlock)
						.TextStyle(InArgs._TextStyle)
					]
				]
				+SOverlay::Slot().HAlign(HAlign_Right)
				[
					SAssignNew(ClearButton, SButton)
					.OnClicked(this, &SKeyBind::ClearClicked)
					.ButtonStyle(SUTStyle::Get(),"UT.Button.Clear")
					.Visibility(EVisibility::Collapsed)
					.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SKeyBind","ClearButtonTT","Clear out the current key.")))
				]
				+ SOverlay::Slot().HAlign(HAlign_Left)
				[
					SAssignNew(AbortButton, SButton)
					.OnClicked(this, &SKeyBind::AbortClicked)
					.ButtonStyle(SUTStyle::Get(), "UT.Button.Abort")
					.Visibility(EVisibility::Collapsed)
					.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SKeyBind", "AbortButtonTT", "Abort setting the key and keep it the same.")))
				]

			];
#endif
		if (InArgs._Key.IsValid())
		{
			Key = InArgs._Key;
			KeyText->SetText(*Key == FKey() ? FString() : Key->ToString());
		}
		bWaitingForKey = false;
	}

	virtual FKey GetKey()
	{
		return *Key;
	}

	virtual void SetKey(FKey NewKey, bool bCanReset = true, bool bNotify = true)
	{
		if (Key.IsValid() )
		{
			bWaitingForKey = false;
			//FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->Show(true);
			FSlateApplication::Get().ClearKeyboardFocus(EKeyboardFocusCause::SetDirectly);
			FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->Lock(nullptr);

			AbortButton->SetVisibility(EVisibility::Collapsed);

			if (NewKey == *Key && bCanReset)
			{
				KeyText->SetText(*Key == FKey() ? FString() : Key->ToString());
				KeyText->SetColorAndOpacity(*Key == DefaultKey ? FLinearColor::Black : NonDefaultKeyColor);

				ClearButton->SetVisibility(EVisibility::Visible);
				return;
			}

			FKey PreviousKey = *Key;
			*Key = NewKey;

			KeyText->SetText(NewKey == FKey() ? FString() : NewKey.ToString());
			KeyText->SetColorAndOpacity( NewKey == DefaultKey ? FLinearColor::Black : NonDefaultKeyColor);
			
			if( bNotify )
			{
				OnKeyBindingChanged.ExecuteIfBound( PreviousKey, *Key);
			}
		}
	}
protected:

	FReply ClearClicked()
	{
		FKey CurrentKey = *Key;
		*Key = FKey();
		KeyText->SetText(FString());
		bWaitingForKey = false;
		//FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->Show(true);
		FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->Lock(nullptr);
		FSlateApplication::Get().ClearKeyboardFocus(EKeyboardFocusCause::SetDirectly);
		ClearButton->SetVisibility(EVisibility::Collapsed);
		AbortButton->SetVisibility(EVisibility::Collapsed);
		return FReply::Handled();
	}

	FReply AbortClicked()
	{
		bWaitingForKey = false;
		FSlateApplication::Get().ClearKeyboardFocus(EKeyboardFocusCause::SetDirectly);
		FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->Lock(nullptr);

		KeyText->SetText(*Key == FKey() ? FString() : Key->ToString());
		KeyText->SetColorAndOpacity(*Key== DefaultKey ? FLinearColor::Black : NonDefaultKeyColor);

		ClearButton->SetVisibility(*Key != FKey() ? EVisibility::Visible : EVisibility::Collapsed);
		AbortButton->SetVisibility(EVisibility::Collapsed);
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		return bWaitingForKey ? FReply::Handled() : FReply::Unhandled();
	}

	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (bWaitingForKey)
		{
			if (InKeyEvent.GetKey() == EKeys::Escape)
			{
				return AbortClicked();
			}

			SetKey(InKeyEvent.GetKey());
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (ClearButton.IsValid() && *Key != FKey() && !bWaitingForKey)
		{
			ClearButton->SetVisibility(EVisibility::Visible);
		}
		SButton::OnMouseEnter(MyGeometry, MouseEvent);
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		if (ClearButton.IsValid())
		{
			ClearButton->SetVisibility(EVisibility::Collapsed);
		}
		SButton::OnMouseLeave(MouseEvent);
	}


	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bWaitingForKey)
		{
			SetKey(MouseEvent.GetWheelDelta() > 0 ? EKeys::MouseScrollUp : EKeys::MouseScrollDown);
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		//Make sure the mouse pointer doesnt leave the button
		if (bWaitingForKey)
		{
		}
		return SButton::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(200.0f, 46.0f);
	}

private:
	FOnKeyBindingChanged OnKeyBindingChanged;
	TSharedPtr<FKey> Key;
	FKey DefaultKey;
	TSharedPtr<STextBlock> KeyText;
	TSharedPtr<SButton> ClearButton;
	TSharedPtr<SButton> AbortButton;
	FVector2D WaitingMousePos;
	bool bWaitingForKey;
	FLinearColor NonDefaultKeyColor;
};

