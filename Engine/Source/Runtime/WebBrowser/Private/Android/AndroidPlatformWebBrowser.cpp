// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"

#if PLATFORM_ANDROID
#include "AndroidPlatformWebBrowser.h"
#include "AndroidWindow.h"
#include "AndroidJava.h"

class SAndroidWebBrowserWidget : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SAndroidWebBrowserWidget)
		: _InitialURL("about:blank")
	{ }

		SLATE_ARGUMENT(FString, InitialURL);
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FVector2D ComputeDesiredSize(float) const override;

	void LoadURL(const FString& NewURL);

	void LoadString(const FString& Contents, const FString& DummyURL);

	void ExecuteJavascript(const FString& Script);

	void Close();

protected:
	// mutable to allow calling JWebView_Update from inside OnPaint (which is const)
	mutable TOptional<FJavaClassObject> JWebView;
	TOptional<FJavaClassMethod> JWebView_Update;
	TOptional<FJavaClassMethod> JWebView_LoadURL;
	TOptional<FJavaClassMethod> JWebView_LoadString;
	TOptional<FJavaClassMethod> JWebView_ExecuteJavascript;
	TOptional<FJavaClassMethod> JWebView_Close;
};

void SAndroidWebBrowserWidget::Construct(const FArguments& Args)
{
	JWebView.Emplace("com/epicgames/ue4/WebViewControl", "()V");

	JWebView_Update = JWebView->GetClassMethod("Update", "(IIII)V");
	JWebView_LoadURL = JWebView->GetClassMethod("LoadURL", "(Ljava/lang/String;)V");
	JWebView_LoadString = JWebView->GetClassMethod("LoadString", "(Ljava/lang/String;Ljava/lang/String;)V");
	JWebView_ExecuteJavascript = JWebView->GetClassMethod("ExecuteJavascript", "(Ljava/lang/String;)V");
	JWebView_Close = JWebView->GetClassMethod("Close", "()V");

	JWebView->CallMethod<void>(JWebView_LoadURL.GetValue(), FJavaClassObject::GetJString(Args._InitialURL));
}

int32 SAndroidWebBrowserWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// Calculate UIScale, which can vary frame-to-frame thanks to device rotation
	// UI Scale is calculated relative to vertical axis of 1280x720 / 720x1280
	float UIScale;
	FPlatformRect ScreenRect = FAndroidWindow::GetScreenRect();
	int32_t ScreenWidth, ScreenHeight;
	FAndroidWindow::CalculateSurfaceSize(FPlatformMisc::GetHardwareWindow(), ScreenWidth, ScreenHeight);
	if (ScreenWidth > ScreenHeight)
	{
		UIScale = (float)ScreenHeight / (ScreenRect.Bottom - ScreenRect.Top);
	}
	else
	{
		UIScale = (float)ScreenHeight / (ScreenRect.Bottom - ScreenRect.Top);
	}

	FVector2D Position = AllottedGeometry.GetAccumulatedRenderTransform().GetTranslation() * UIScale;
	FVector2D Size = TransformVector(AllottedGeometry.GetAccumulatedRenderTransform(), AllottedGeometry.GetLocalSize()) * UIScale;

	// Convert position to integer coordinates
	FIntPoint IntPos(FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y));
	// Convert size to integer taking the rounding of position into account to avoid double round-down or double round-up causing a noticeable error.
	FIntPoint IntSize = FIntPoint(FMath::RoundToInt(Position.X + Size.X), FMath::RoundToInt(Size.Y + Position.Y)) - IntPos;

	JWebView->CallMethod<void>(JWebView_Update.GetValue(), IntPos.X, IntPos.Y, IntSize.X, IntSize.Y);

	return LayerId;
}

FVector2D SAndroidWebBrowserWidget::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(640, 480);
}

void SAndroidWebBrowserWidget::LoadURL(const FString& NewURL)
{
	JWebView->CallMethod<void>(JWebView_LoadURL.GetValue(), FJavaClassObject::GetJString(NewURL));
}

void SAndroidWebBrowserWidget::LoadString(const FString& Contents, const FString& DummyURL)
{
	JWebView->CallMethod<void>(JWebView_LoadString.GetValue(), FJavaClassObject::GetJString(Contents), FJavaClassObject::GetJString(DummyURL));
}

void SAndroidWebBrowserWidget::ExecuteJavascript(const FString& JavaScript)
{
	JWebView->CallMethod<void>(JWebView_ExecuteJavascript.GetValue(), FJavaClassObject::GetJString(JavaScript));
}

void SAndroidWebBrowserWidget::Close()
{
	JWebView->CallMethod<void>(JWebView_Close.GetValue());
}

FWebBrowserWindow::FWebBrowserWindow(FString InUrl, TOptional<FString> InContentsToLoad, bool InShowErrorMessage, bool InThumbMouseButtonNavigation, bool InUseTransparency)
	: CurrentUrl(MoveTemp(InUrl))
	, ContentsToLoad(MoveTemp(InContentsToLoad))
{
}

FWebBrowserWindow::~FWebBrowserWindow()
{
	CloseBrowser(true);
}

void FWebBrowserWindow::LoadURL(FString NewURL)
{
	BrowserWidget->LoadURL(NewURL);
}

void FWebBrowserWindow::LoadString(FString Contents, FString DummyURL)
{
	BrowserWidget->LoadString(Contents, DummyURL);
}

TSharedRef<SWidget> FWebBrowserWindow::CreateWidget()
{
	TSharedRef<SAndroidWebBrowserWidget> BrowserWidgetRef =
		SNew(SAndroidWebBrowserWidget)
		.InitialURL(CurrentUrl);

	BrowserWidget = BrowserWidgetRef;
	return BrowserWidgetRef;
}

void FWebBrowserWindow::SetViewportSize(FIntPoint WindowSize, FIntPoint WindowPos)
{
}

FSlateShaderResource* FWebBrowserWindow::GetTexture(bool bIsPopup /*= false*/)
{
	return nullptr;
}

bool FWebBrowserWindow::IsValid() const
{
	return false;
}

bool FWebBrowserWindow::IsInitialized() const
{
	return true;
}

bool FWebBrowserWindow::IsClosing() const
{
	return false;
}

EWebBrowserDocumentState FWebBrowserWindow::GetDocumentLoadingState() const
{
	return EWebBrowserDocumentState::Loading;
}

FString FWebBrowserWindow::GetTitle() const
{
	return "";
}

FString FWebBrowserWindow::GetUrl() const
{
	return "";
}

bool FWebBrowserWindow::OnKeyDown(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyUp(const FKeyEvent& InKeyEvent)
{
	return false;
}

bool FWebBrowserWindow::OnKeyChar(const FCharacterEvent& InCharacterEvent)
{
	return false;
}

FReply FWebBrowserWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

FReply FWebBrowserWindow::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bIsPopup)
{
	return FReply::Unhandled();
}

void FWebBrowserWindow::OnFocus(bool SetFocus, bool bIsPopup)
{
}

void FWebBrowserWindow::OnCaptureLost()
{
}

bool FWebBrowserWindow::CanGoBack() const
{
	return false;
}

void FWebBrowserWindow::GoBack()
{
}

bool FWebBrowserWindow::CanGoForward() const
{
	return false;
}

void FWebBrowserWindow::GoForward()
{
}

bool FWebBrowserWindow::IsLoading() const
{
	return false;
}

void FWebBrowserWindow::Reload()
{
}

void FWebBrowserWindow::StopLoad()
{
}

void FWebBrowserWindow::GetSource(TFunction<void (const FString&)> Callback) const
{
	Callback(FString());
}

int FWebBrowserWindow::GetLoadError()
{
	return 0;
}

void FWebBrowserWindow::SetIsDisabled(bool bValue)
{
}


void FWebBrowserWindow::ExecuteJavascript(const FString& Script)
{
	BrowserWidget->ExecuteJavascript(Script);
}

void FWebBrowserWindow::CloseBrowser(bool bForce)
{
	BrowserWidget->Close();
}

void FWebBrowserWindow::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent /*= true*/)
{
}

void FWebBrowserWindow::UnbindUObject(const FString& Name, UObject* Object /*= nullptr*/, bool bIsPermanent /*= true*/)
{
}

#endif
