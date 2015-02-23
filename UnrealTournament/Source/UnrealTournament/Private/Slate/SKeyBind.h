// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"

class SKeyBind : public SButton
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
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SButton::Construct(SButton::FArguments()
			.ButtonStyle(InArgs._ButtonStyle)
			.TextStyle(InArgs._TextStyle)
			.HAlign(InArgs._HAlign)
			.VAlign(InArgs._VAlign)
			.ContentPadding(InArgs._ContentPadding)
			.DesiredSizeScale(InArgs._DesiredSizeScale)
			.ContentScale(InArgs._ContentScale)
			.ButtonColorAndOpacity(InArgs._ButtonColorAndOpacity)
			.ForegroundColor(InArgs._ForegroundColor)
		);

		ChildSlot
			[
				SAssignNew(KeyText, STextBlock)
				.TextStyle(InArgs._TextStyle)
			];
		if (InArgs._Key.IsValid())
		{
			Key = InArgs._Key;
			KeyText->SetText(*Key == FKey() ? FString() : Key->ToString());
		}
		bWaitingForKey = false;
	}

		virtual void SetKey(FKey NewKey, bool bCanReset = true)
		{
			if (Key.IsValid())
			{
				if (NewKey == *Key && bCanReset)
				{
					NewKey = FKey();
				}
				*Key = NewKey;
				KeyText->SetText(NewKey == FKey() ? FString() : NewKey.ToString());
				KeyText->SetColorAndOpacity(FLinearColor::White);
				bWaitingForKey = false;
				FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->Show(true);
				FSlateApplication::Get().ClearKeyboardFocus(EKeyboardFocusCause::SetDirectly);
			}
		}
protected:

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (bWaitingForKey)
		{
			SetKey(InKeyEvent.GetKey());
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bWaitingForKey)
		{
			SetKey(MouseEvent.GetEffectingButton());
			return FReply::Handled();
		}
		else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			//Get the center of the widget ao we can lock our mouse there
			FSlateRect Rect = MyGeometry.GetClippingRect();
			WaitingMousePos.X = (Rect.Left + Rect.Right) * 0.5f;
			WaitingMousePos.Y = (Rect.Top + Rect.Bottom) * 0.5f;
			FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->SetPosition(WaitingMousePos.X, WaitingMousePos.Y);

			KeyText->SetText(NSLOCTEXT("SKeyBind", "PressAnyKey", "** Press Any Button **"));
			KeyText->SetColorAndOpacity(FLinearColor(FColor(0,255,0,255)));
			bWaitingForKey = true;
			FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->Show(false);
			return FReply::Handled();
		}
		return FReply::Unhandled();
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
			FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->SetPosition(WaitingMousePos.X, WaitingMousePos.Y);
		}
		return SButton::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	}

	virtual FVector2D ComputeDesiredSize() const override
	{
		return FVector2D(200.0f, 46.0f);
	}

private:
	TSharedPtr<FKey> Key;
	TSharedPtr<STextBlock> KeyText;
	FVector2D WaitingMousePos;
	bool bWaitingForKey;
};

