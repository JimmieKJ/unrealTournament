// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "WebBrowserViewport.h"
#include "IWebBrowserWindow.h"

FIntPoint FWebBrowserViewport::GetSize() const
{
	return (WebBrowserWindow->GetTexture() != nullptr)
		? FIntPoint(WebBrowserWindow->GetTexture()->GetWidth(), WebBrowserWindow->GetTexture()->GetHeight())
		: FIntPoint();
}

FSlateShaderResource* FWebBrowserViewport::GetViewportRenderTargetTexture() const
{
	return WebBrowserWindow->GetTexture();
}

void FWebBrowserViewport::Tick( const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime )
{
	// Calculate max corner of the viewport using same method as Slate
	FVector2D MaxPos = AllottedGeometry.AbsolutePosition + AllottedGeometry.GetLocalSize();
	// Get size by subtracting as int to avoid incorrect rounding when size and position are .5
	WebBrowserWindow->SetViewportSize(MaxPos.IntPoint() - AllottedGeometry.AbsolutePosition.IntPoint());
}

bool FWebBrowserViewport::RequiresVsync() const
{
	return false;
}

FCursorReply FWebBrowserViewport::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent )
{
	// TODO: retrieve cursor type from WebBrowserWindow if we can figure that out from CefCursorHandle
	return FCursorReply::Unhandled();
}

FReply FWebBrowserViewport::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Capture mouse on left button down so that you can drag out of the viewport
	WebBrowserWindow->OnMouseButtonDown(MyGeometry, MouseEvent);
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && ViewportWidget.IsValid())
	{
		return FReply::Handled().CaptureMouse(ViewportWidget.Pin().ToSharedRef());
	}
	return FReply::Handled();
}

FReply FWebBrowserViewport::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Release mouse capture when left button released
	WebBrowserWindow->OnMouseButtonUp(MyGeometry, MouseEvent);
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

void FWebBrowserViewport::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
}

void FWebBrowserViewport::OnMouseLeave(const FPointerEvent& MouseEvent)
{
}

FReply FWebBrowserViewport::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	WebBrowserWindow->OnMouseMove(MyGeometry, MouseEvent);
	return FReply::Handled();
}

FReply FWebBrowserViewport::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	WebBrowserWindow->OnMouseWheel(MyGeometry, MouseEvent);
	return FReply::Handled();
}

FReply FWebBrowserViewport::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	WebBrowserWindow->OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply FWebBrowserViewport::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	WebBrowserWindow->OnKeyDown(InKeyEvent);
	return FReply::Handled();
}

FReply FWebBrowserViewport::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	WebBrowserWindow->OnKeyUp(InKeyEvent);
	return FReply::Handled();
}

FReply FWebBrowserViewport::OnKeyChar( const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent )
{
	WebBrowserWindow->OnKeyChar(InCharacterEvent);
	return FReply::Handled();
}

FReply FWebBrowserViewport::OnFocusReceived(const FFocusEvent& InFocusEvent)
{
	WebBrowserWindow->OnFocus(true);
	return FReply::Handled();
}

void FWebBrowserViewport::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	WebBrowserWindow->OnFocus(false);
}
