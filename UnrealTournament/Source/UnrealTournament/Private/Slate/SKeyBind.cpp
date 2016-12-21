// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"

#if PLATFORM_WINDOWS
#undef ERROR_SUCCESS
#undef ERROR_IO_PENDING
#undef E_NOTIMPL
#undef E_FAIL
#undef S_OK
#include "WindowsHWrapper.h"
#endif

#include "SKeyBind.h"


FReply SKeyBind::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bWaitingForKey)
	{
		ClearButton->SetVisibility(EVisibility::Visible);
		SetKey(MouseEvent.GetEffectingButton());
		return FReply::Handled();
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		//Get the center of the widget so we can lock our mouse there
		FSlateRect Rect = MyGeometry.GetClippingRect();
		WaitingMousePos.X = (Rect.Left + Rect.Right) * 0.5f;
		WaitingMousePos.Y = (Rect.Top + Rect.Bottom) * 0.5f;
		FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->SetPosition(WaitingMousePos.X, WaitingMousePos.Y);

		RECT ClipRect;
		ClipRect.left = FMath::RoundToInt(Rect.Left);
		ClipRect.top = FMath::RoundToInt(Rect.Top);
		ClipRect.right = FMath::TruncToInt(Rect.Right);
		ClipRect.bottom = FMath::TruncToInt(Rect.Bottom);

		FSlateApplication::Get().GetPlatformApplication().Get()->Cursor->Lock(&ClipRect);

		ClearButton->SetVisibility(EVisibility::Collapsed);
		AbortButton->SetVisibility(EVisibility::Visible);

		KeyText->SetText(NSLOCTEXT("SKeyBind", "PressAnyKey", "** Press Any Button **"));
		KeyText->SetColorAndOpacity(FLinearColor(FColor(0, 0, 255, 255)));
		bWaitingForKey = true;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}
