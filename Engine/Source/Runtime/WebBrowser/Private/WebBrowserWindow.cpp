// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "WebBrowserWindow.h"
#include "SlateCore.h"
#include "SlateBasics.h"
#include "RHI.h"

#if WITH_CEF3
FWebBrowserWindow::FWebBrowserWindow(FIntPoint InViewportSize, FString InUrl, TOptional<FString> InContentsToLoad, bool bInShowErrorMessage)
	: DocumentState(EWebBrowserDocumentState::NoDocument)
	, UpdatableTexture(nullptr)
	, CurrentUrl(InUrl)
	, ViewportSize(InViewportSize)
	, bIsClosing(false)
	, bHasBeenPainted(false)
	, ContentsToLoad(InContentsToLoad)
	, ShowErrorMessage(bInShowErrorMessage)
{
	TextureData.Reserve(ViewportSize.X * ViewportSize.Y * 4);
	TextureData.SetNumZeroed(ViewportSize.X * ViewportSize.Y * 4);
    bTextureDataDirty = true;

	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer().IsValid())
	{
		UpdatableTexture = FSlateApplication::Get().GetRenderer()->CreateUpdatableTexture(ViewportSize.X, ViewportSize.Y);
	}
}

FWebBrowserWindow::~FWebBrowserWindow()
{
	CloseBrowser();

	if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer().IsValid() && UpdatableTexture != nullptr)
	{
		FSlateApplication::Get().GetRenderer()->ReleaseUpdatableTexture(UpdatableTexture);
	}
	UpdatableTexture = nullptr;
}

void FWebBrowserWindow::LoadURL(FString NewURL)
{
	if (IsValid())
	{
		CefRefPtr<CefFrame> MainFrame = InternalCefBrowser->GetMainFrame();
		if (MainFrame.get() != nullptr)
		{
			CefString URL = *NewURL;
			MainFrame->LoadURL(URL);
		}
	}
}

void FWebBrowserWindow::LoadString(FString Contents, FString DummyURL)
{
	if (IsValid())
	{
		CefRefPtr<CefFrame> MainFrame = InternalCefBrowser->GetMainFrame();
		if (MainFrame.get() != nullptr)
		{
			CefString StringVal = *Contents;
			CefString URL = *DummyURL;
			MainFrame->LoadString(StringVal, URL);
		}
	}
}

void FWebBrowserWindow::SetViewportSize(FIntPoint WindowSize)
{
	const int32 MaxSize = GetMax2DTextureDimension();
	WindowSize.X = FMath::Min(WindowSize.X, MaxSize);
	WindowSize.Y = FMath::Min(WindowSize.Y, MaxSize);

	// Ignore sizes that can't be seen as it forces CEF to re-render whole image
	if (WindowSize.X > 0 && WindowSize.Y > 0 && ViewportSize != WindowSize)
	{
		FIntPoint OldViewportSize = MoveTemp(ViewportSize);
		TArray<uint8> OldTextureData = MoveTemp(TextureData);
		ViewportSize = MoveTemp(WindowSize);
		TextureData.SetNumZeroed(ViewportSize.X * ViewportSize.Y * 4);

		// copy row by row to avoid texture distortion
		const int32 WriteWidth = FMath::Min(OldViewportSize.X, ViewportSize.X) * 4;
		const int32 WriteHeight = FMath::Min(OldViewportSize.Y, ViewportSize.Y);
		for (int32 RowIndex = 0; RowIndex < WriteHeight; ++RowIndex)
		{
			FMemory::Memcpy(TextureData.GetData() + ViewportSize.X * RowIndex * 4, OldTextureData.GetData() + OldViewportSize.X * RowIndex * 4, WriteWidth);
		}

		if (UpdatableTexture != nullptr)
		{
			UpdatableTexture->ResizeTexture(ViewportSize.X, ViewportSize.Y);
			UpdatableTexture->UpdateTextureThreadSafe(TextureData);
            bTextureDataDirty = false;
		}
		if (IsValid())
		{
			InternalCefBrowser->GetHost()->WasResized();
		}
	}
}

FSlateShaderResource* FWebBrowserWindow::GetTexture()
{
	if (UpdatableTexture != nullptr)
	{
        if (bTextureDataDirty)
        {
            UpdatableTexture->UpdateTextureThreadSafe(TextureData);
            bTextureDataDirty = false;
        }
		return UpdatableTexture->GetSlateResource();
	}
	return nullptr;
}

bool FWebBrowserWindow::IsValid() const
{
	return InternalCefBrowser.get() != nullptr;
}

bool FWebBrowserWindow::HasBeenPainted() const
{
	return bHasBeenPainted;
}

bool FWebBrowserWindow::IsClosing() const
{
	return bIsClosing;
}

EWebBrowserDocumentState FWebBrowserWindow::GetDocumentLoadingState() const
{
	return DocumentState;
}

FString FWebBrowserWindow::GetTitle() const
{
	return Title;
}

FString FWebBrowserWindow::GetUrl() const
{
	if (InternalCefBrowser != nullptr)
	{
		CefRefPtr<CefFrame> MainFrame = InternalCefBrowser->GetMainFrame();

		if (MainFrame != nullptr)
		{
			return CurrentUrl;
		}
	}

	return FString();
}

void FWebBrowserWindow::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	if (IsValid())
	{
		CefKeyEvent KeyEvent;
#if PLATFORM_MAC
		KeyEvent.native_key_code = InKeyEvent.GetKeyCode();
		KeyEvent.character = InKeyEvent.GetCharacter();
#else
		KeyEvent.windows_key_code = InKeyEvent.GetKeyCode();
#endif
		// TODO: Figure out whether this is a system key if we come across problems
		/*KeyEvent.is_system_key = message == WM_SYSCHAR ||
			message == WM_SYSKEYDOWN ||
			message == WM_SYSKEYUP;*/

		KeyEvent.type = KEYEVENT_RAWKEYDOWN;
		KeyEvent.modifiers = GetCefKeyboardModifiers(InKeyEvent);

		InternalCefBrowser->GetHost()->SendKeyEvent(KeyEvent);
	}
}

void FWebBrowserWindow::OnKeyUp(const FKeyEvent& InKeyEvent)
{
	if (IsValid())
	{
		CefKeyEvent KeyEvent;
#if PLATFORM_MAC
		KeyEvent.native_key_code = InKeyEvent.GetKeyCode();
		KeyEvent.character = InKeyEvent.GetCharacter();
#else
		KeyEvent.windows_key_code = InKeyEvent.GetKeyCode();
#endif
		// TODO: Figure out whether this is a system key if we come across problems
		/*KeyEvent.is_system_key = message == WM_SYSCHAR ||
			message == WM_SYSKEYDOWN ||
			message == WM_SYSKEYUP;*/

		KeyEvent.type = KEYEVENT_KEYUP;
		KeyEvent.modifiers = GetCefKeyboardModifiers(InKeyEvent);

		InternalCefBrowser->GetHost()->SendKeyEvent(KeyEvent);
	}
}

void FWebBrowserWindow::OnKeyChar(const FCharacterEvent& InCharacterEvent)
{
	if (IsValid())
	{
		CefKeyEvent KeyEvent;
#if PLATFORM_MAC
		KeyEvent.character = InCharacterEvent.GetCharacter();
#else
		KeyEvent.windows_key_code = InCharacterEvent.GetCharacter();
#endif
		// TODO: Figure out whether this is a system key if we come across problems
		/*KeyEvent.is_system_key = message == WM_SYSCHAR ||
			message == WM_SYSKEYDOWN ||
			message == WM_SYSKEYUP;*/

		KeyEvent.type = KEYEVENT_CHAR;
		KeyEvent.modifiers = GetCefInputModifiers(InCharacterEvent);

		InternalCefBrowser->GetHost()->SendKeyEvent(KeyEvent);
	}
}

void FWebBrowserWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsValid())
	{
		FKey Button = MouseEvent.GetEffectingButton();
		CefBrowserHost::MouseButtonType Type =
			(Button == EKeys::LeftMouseButton ? MBT_LEFT : (
			Button == EKeys::RightMouseButton ? MBT_RIGHT : MBT_MIDDLE));

		CefMouseEvent Event;
		FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		Event.x = LocalPos.X;
		Event.y = LocalPos.Y;
		Event.modifiers = GetCefMouseModifiers(MouseEvent);

		InternalCefBrowser->GetHost()->SendMouseClickEvent(Event, Type, false,1);
	}
}

void FWebBrowserWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsValid())
	{
		FKey Button = MouseEvent.GetEffectingButton();
		CefBrowserHost::MouseButtonType Type =
			(Button == EKeys::LeftMouseButton ? MBT_LEFT : (
			Button == EKeys::RightMouseButton ? MBT_RIGHT : MBT_MIDDLE));

		CefMouseEvent Event;
		FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		Event.x = LocalPos.X;
		Event.y = LocalPos.Y;
		Event.modifiers = GetCefMouseModifiers(MouseEvent);

		InternalCefBrowser->GetHost()->SendMouseClickEvent(Event, Type, true, 1);
	}
}

void FWebBrowserWindow::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsValid())
	{
		FKey Button = MouseEvent.GetEffectingButton();
		CefBrowserHost::MouseButtonType Type =
			(Button == EKeys::LeftMouseButton ? MBT_LEFT : (
			Button == EKeys::RightMouseButton ? MBT_RIGHT : MBT_MIDDLE));

		CefMouseEvent Event;
		FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		Event.x = LocalPos.X;
		Event.y = LocalPos.Y;
		Event.modifiers = GetCefMouseModifiers(MouseEvent);

		InternalCefBrowser->GetHost()->SendMouseClickEvent(Event, Type, false, 2);
	}
}

void FWebBrowserWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (IsValid())
	{
		CefMouseEvent Event;
		FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		Event.x = LocalPos.X;
		Event.y = LocalPos.Y;
		Event.modifiers = GetCefMouseModifiers(MouseEvent);

		InternalCefBrowser->GetHost()->SendMouseMoveEvent(Event, false);
	}
}

void FWebBrowserWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(IsValid())
	{
		// The original delta is reduced so this should bring it back to what CEF expects
		const float SpinFactor = 120.0f;
		const float TrueDelta = MouseEvent.GetWheelDelta() * SpinFactor;
		CefMouseEvent Event;
		FVector2D LocalPos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		Event.x = LocalPos.X;
		Event.y = LocalPos.Y;
		Event.modifiers = GetCefMouseModifiers(MouseEvent);
		
		InternalCefBrowser->GetHost()->SendMouseWheelEvent(Event,
															MouseEvent.IsShiftDown() ? TrueDelta : 0,
															!MouseEvent.IsShiftDown() ? TrueDelta : 0);
	}
}

void FWebBrowserWindow::OnFocus(bool SetFocus)
{
	if (IsValid())
	{
		InternalCefBrowser->GetHost()->SendFocusEvent(SetFocus);
	}
}

void FWebBrowserWindow::OnCaptureLost()
{
	if (IsValid())
	{
		InternalCefBrowser->GetHost()->SendCaptureLostEvent();
	}
}

bool FWebBrowserWindow::CanGoBack() const
{
	if (IsValid())
	{
		return InternalCefBrowser->CanGoBack();
	}
	return false;
}

void FWebBrowserWindow::GoBack()
{
	if (IsValid())
	{
		InternalCefBrowser->GoBack();
	}
}

bool FWebBrowserWindow::CanGoForward() const
{
	if (IsValid())
	{
		return InternalCefBrowser->CanGoForward();
	}
	return false;
}

void FWebBrowserWindow::GoForward()
{
	if (IsValid())
	{
		InternalCefBrowser->GoForward();
	}
}

bool FWebBrowserWindow::IsLoading() const
{
	if (IsValid())
	{
		return InternalCefBrowser->IsLoading();
	}
	return false;
}

void FWebBrowserWindow::Reload()
{
	if (IsValid())
	{
		InternalCefBrowser->Reload();
	}
}

void FWebBrowserWindow::StopLoad()
{
	if (IsValid())
	{
		InternalCefBrowser->StopLoad();
	}
}

void FWebBrowserWindow::ExecuteJavascript(const FString& Script)
{
	if (IsValid())
	{
		CefRefPtr<CefFrame> frame = InternalCefBrowser->GetMainFrame();
		frame->ExecuteJavaScript(TCHAR_TO_ANSI(*Script), frame->GetURL(), 0);
	}
}

void FWebBrowserWindow::SetHandler(CefRefPtr<FWebBrowserHandler> InHandler)
{
	if (InHandler.get())
	{
		Handler = InHandler;
		Handler->SetBrowserWindow(SharedThis(this));
		Handler->SetShowErrorMessage(ShowErrorMessage);
	}
}

void FWebBrowserWindow::CloseBrowser()
{
	bIsClosing = true;
	if (IsValid())
	{
		InternalCefBrowser->GetHost()->CloseBrowser(false);
		InternalCefBrowser = nullptr;
		Handler = nullptr;
	}
}

void FWebBrowserWindow::BindCefBrowser(CefRefPtr<CefBrowser> Browser)
{
	InternalCefBrowser = Browser;
	// Need to wait until this point if we want to start with a page loaded from a string
	if (ContentsToLoad.IsSet())
	{
		LoadString(ContentsToLoad.GetValue(), CurrentUrl);
	}
}

void FWebBrowserWindow::SetTitle(const CefString& InTitle)
{
	Title = InTitle.ToWString().c_str();
	TitleChangedEvent.Broadcast(Title);
}

void FWebBrowserWindow::SetUrl(const CefString& Url)
{
	CurrentUrl = Url.ToWString().c_str();
	OnUrlChanged().Broadcast(CurrentUrl);
}

bool FWebBrowserWindow::GetViewRect(CefRect& Rect)
{
	Rect.x = 0;
	Rect.y = 0;
	Rect.width = ViewportSize.X;
	Rect.height = ViewportSize.Y;

	return true;
}

void FWebBrowserWindow::NotifyDocumentError()
{
	DocumentState = EWebBrowserDocumentState::Error;
	DocumentStateChangedEvent.Broadcast(DocumentState);
}

void FWebBrowserWindow::NotifyDocumentLoadingStateChange(bool IsLoading)
{
	EWebBrowserDocumentState NewState = IsLoading
		? EWebBrowserDocumentState::Loading
		: EWebBrowserDocumentState::Completed;

	if (DocumentState != EWebBrowserDocumentState::Error)
	{
		DocumentState = NewState;
	}

	DocumentStateChangedEvent.Broadcast(NewState);
}

void FWebBrowserWindow::OnPaint(CefRenderHandler::PaintElementType Type, const CefRenderHandler::RectList& DirtyRects, const void* Buffer, int Width, int Height)
{
	const int32 BufferSize = Width*Height*4;
	if (BufferSize == TextureData.Num() && DirtyRects.size() == 1 && DirtyRects[0].width == Width && DirtyRects[0].height == Height)
	{
		FMemory::Memcpy(TextureData.GetData(), Buffer, BufferSize);
	}
	else
	{
        for (CefRect Rect : DirtyRects)
        {
            // Skip rect if fully outside the viewport
            if (Rect.x > ViewportSize.X || Rect.y > ViewportSize.Y)
                continue;
            
            const int32 WriteWidth = (FMath::Min(Rect.x + Rect.width, ViewportSize.X) - Rect.x) * 4;
            const int32 WriteHeight = FMath::Min(Rect.y + Rect.height, ViewportSize.Y);
            // copy row by row to skip pixels outside the rect
            for (int32 RowIndex = Rect.y; RowIndex < WriteHeight; ++RowIndex)
            {
                FMemory::Memcpy(TextureData.GetData() + ( ViewportSize.X * RowIndex + Rect.x ) * 4 , static_cast<const uint8*>(Buffer) + (Width * RowIndex + Rect.x) * 4, WriteWidth);
            }
        }
	}

    bTextureDataDirty = true;
    bHasBeenPainted = true;

    NeedsRedrawEvent.Broadcast();
}

void FWebBrowserWindow::OnCursorChange(CefCursorHandle Cursor, CefRenderHandler::CursorType Type, const CefCursorInfo& CustomCursorInfo)
{
	// TODO: Figure out Unreal cursor type from CefRenderHandler::CursorType,
	//::SetCursor( Cursor );
}

int32 FWebBrowserWindow::GetCefKeyboardModifiers(const FKeyEvent& KeyEvent)
{
	int32 Modifiers = GetCefInputModifiers(KeyEvent);

	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::LeftAlt ||
		Key == EKeys::LeftCommand ||
		Key == EKeys::LeftControl ||
		Key == EKeys::LeftShift)
	{
		Modifiers |= EVENTFLAG_IS_LEFT;
	}
	if (Key == EKeys::RightAlt ||
		Key == EKeys::RightCommand ||
		Key == EKeys::RightControl ||
		Key == EKeys::RightShift)
	{
		Modifiers |= EVENTFLAG_IS_RIGHT;
	}
	if (Key == EKeys::NumPadZero ||
		Key == EKeys::NumPadOne ||
		Key == EKeys::NumPadTwo ||
		Key == EKeys::NumPadThree ||
		Key == EKeys::NumPadFour ||
		Key == EKeys::NumPadFive ||
		Key == EKeys::NumPadSix ||
		Key == EKeys::NumPadSeven ||
		Key == EKeys::NumPadEight ||
		Key == EKeys::NumPadNine)
	{
		Modifiers |= EVENTFLAG_IS_KEY_PAD;
	}

	return Modifiers;
}

int32 FWebBrowserWindow::GetCefMouseModifiers(const FPointerEvent& InMouseEvent)
{
	int32 Modifiers = GetCefInputModifiers(InMouseEvent);

	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		Modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	}
	if (InMouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton))
	{
		Modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	}
	if (InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		Modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
	}

	return Modifiers;
}

int32 FWebBrowserWindow::GetCefInputModifiers(const FInputEvent& InputEvent)
{
	int32 Modifiers = 0;

	if (InputEvent.IsShiftDown())
	{
		Modifiers |= EVENTFLAG_SHIFT_DOWN;
	}
	if (InputEvent.IsControlDown())
	{
		Modifiers |= EVENTFLAG_CONTROL_DOWN;
	}
	if (InputEvent.IsAltDown())
	{
		Modifiers |= EVENTFLAG_ALT_DOWN;
	}
	if (InputEvent.IsCommandDown())
	{
		Modifiers |= EVENTFLAG_COMMAND_DOWN;
	}
	if (InputEvent.AreCapsLocked())
	{
		Modifiers |= EVENTFLAG_CAPS_LOCK_ON;
	}
	// TODO: Add function for this if necessary
	/*if (InputEvent.AreNumsLocked())
	{
		Modifiers |= EVENTFLAG_NUM_LOCK_ON;
	}*/

	return Modifiers;
}

bool FWebBrowserWindow::OnQuery(int64 QueryId, const CefString& Request, bool Persistent, CefRefPtr<CefMessageRouterBrowserSide::Callback> Callback)
{
	if (OnJSQueryReceived().IsBound())
	{
		FString QueryString = Request.ToWString().c_str();
		FJSQueryResultDelegate Delegate = FJSQueryResultDelegate::CreateLambda(
			[Callback](int ErrorCode, FString Message)
		{
			CefString MessageString = *Message;
			if (ErrorCode == 0)
			{
				Callback->Success(MessageString);
			}
			else
			{
				Callback->Failure(ErrorCode, MessageString);
			}
		}
		);
		return OnJSQueryReceived().Execute(QueryId, QueryString, Persistent, Delegate);
	}
	return false;
}

void FWebBrowserWindow::OnQueryCanceled(int64 QueryId)
{
	OnJSQueryCanceled().ExecuteIfBound(QueryId);
}

bool FWebBrowserWindow::OnCefBeforeBrowse(CefRefPtr<CefRequest> Request, bool IsRedirect)
{
	if (OnBeforeBrowse().IsBound())
	{
		FString URL = Request->GetURL().ToWString().c_str();
		return OnBeforeBrowse().Execute(URL, IsRedirect);
	}

	return false;
}

bool FWebBrowserWindow::OnCefBeforePopup(const CefString& Target_Url, const CefString& Target_Frame_Name)
{
	if (OnBeforePopup().IsBound())
	{
		FString URL = Target_Url.ToWString().c_str();
		FString FrameName = Target_Frame_Name.ToWString().c_str();
		return OnBeforePopup().Execute(URL, FrameName);
	}

	return false;
}

#endif
