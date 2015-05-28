// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "SWebBrowser.h"
#include "SThrobber.h"
#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"
#include "IWebBrowserWindow.h"
#include "WebBrowserViewport.h"

#define LOCTEXT_NAMESPACE "WebBrowser"

SWebBrowser::SWebBrowser()
{
}

void SWebBrowser::Construct(const FArguments& InArgs)
{
	OnLoadCompleted = InArgs._OnLoadCompleted;
	OnLoadError = InArgs._OnLoadError;
	OnLoadStarted = InArgs._OnLoadStarted;
	OnTitleChanged = InArgs._OnTitleChanged;
	OnUrlChanged = InArgs._OnUrlChanged;
	OnJSQueryReceived = InArgs._OnJSQueryReceived;
	OnJSQueryCanceled = InArgs._OnJSQueryCanceled;
	OnBeforeBrowse = InArgs._OnBeforeBrowse;
	OnBeforePopup = InArgs._OnBeforePopup;
	AddressBarUrl = FText::FromString(InArgs._InitialURL);
	IsHandlingRedraw = false;
	

	void* OSWindowHandle = nullptr;
	if (InArgs._ParentWindow.IsValid())
	{
		TSharedPtr<FGenericWindow> NativeWindow = InArgs._ParentWindow->GetNativeWindow();
		OSWindowHandle = NativeWindow->GetOSWindowHandle();
	}

	BrowserWindow = IWebBrowserModule::Get().GetSingleton()->CreateBrowserWindow(
		OSWindowHandle,
		InArgs._InitialURL,
		InArgs._ViewportSize.Get().X,
		InArgs._ViewportSize.Get().Y,
		InArgs._SupportsTransparency,
		InArgs._ContentsToLoad,
		InArgs._ShowErrorMessage
	);

	TSharedPtr<SViewport> ViewportWidget;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SHorizontalBox)
				.Visibility(InArgs._ShowControls ? EVisibility::Visible : EVisibility::Collapsed)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Back","Back"))
					.IsEnabled(this, &SWebBrowser::CanGoBack)
					.OnClicked(this, &SWebBrowser::OnBackClicked)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Forward", "Forward"))
					.IsEnabled(this, &SWebBrowser::CanGoForward)
					.OnClicked(this, &SWebBrowser::OnForwardClicked)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(this, &SWebBrowser::GetReloadButtonText)
					.OnClicked(this, &SWebBrowser::OnReloadClicked)
				]
				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(5)
				[
					SNew(STextBlock)
					.Visibility(InArgs._ShowAddressBar ? EVisibility::Collapsed : EVisibility::Visible )
					.Text(this, &SWebBrowser::GetTitleText)
					.Justification(ETextJustify::Right)
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			.Padding(5.f, 0.f)
			[
				// @todo: A proper addressbar widget should go here, for now we use a simple textbox.
				SAssignNew(InputText, SEditableTextBox)
				.Visibility(InArgs._ShowAddressBar ? EVisibility::Visible : EVisibility::Collapsed)
				.OnTextCommitted(this, &SWebBrowser::OnUrlTextCommitted)
				.Text(this, &SWebBrowser::GetAddressBarUrlText)
				.SelectAllTextWhenFocused(true)
				.ClearKeyboardFocusOnCommit(true)
				.RevertTextOnEscape(true)
			]
		]
		+SVerticalBox::Slot()
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SAssignNew(ViewportWidget, SViewport)
				.ViewportSize(InArgs._ViewportSize)
				.EnableGammaCorrection(false)
				.EnableBlending(InArgs._SupportsTransparency)
				.IgnoreTextureAlpha(!InArgs._SupportsTransparency)
				.Visibility(this, &SWebBrowser::GetViewportVisibility)
			]
			+SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SCircularThrobber)
				.Radius(10.0f)
				.ToolTipText(LOCTEXT("LoadingThrobberToolTip", "Loading page..."))
				.Visibility(this, &SWebBrowser::GetLoadingThrobberVisibility)
			]
		]
	];

	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnJSQueryReceived().BindSP(this, &SWebBrowser::HandleJSQueryReceived);
		BrowserWindow->OnJSQueryCanceled().BindSP(this, &SWebBrowser::HandleJSQueryCanceled);
		BrowserWindow->OnBeforeBrowse().BindSP(this, &SWebBrowser::HandleBeforeBrowse);
		BrowserWindow->OnBeforePopup().BindSP(this, &SWebBrowser::HandleBeforePopup);
		BrowserWindow->OnDocumentStateChanged().AddSP(this, &SWebBrowser::HandleBrowserWindowDocumentStateChanged);
		BrowserWindow->OnNeedsRedraw().AddSP(this, &SWebBrowser::HandleBrowserWindowNeedsRedraw);
		BrowserWindow->OnTitleChanged().AddSP(this, &SWebBrowser::HandleTitleChanged);
		BrowserWindow->OnUrlChanged().AddSP(this, &SWebBrowser::HandleUrlChanged);
		BrowserViewport = MakeShareable(new FWebBrowserViewport(BrowserWindow, ViewportWidget));
		ViewportWidget->SetViewportInterface(BrowserViewport.ToSharedRef());
	}
}

void SWebBrowser::LoadURL(FString NewURL)
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->LoadURL(NewURL);
	}
}

void SWebBrowser::LoadString(FString Contents, FString DummyURL)
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->LoadString(Contents, DummyURL);
	}
}

FText SWebBrowser::GetTitleText() const
{
	if (BrowserWindow.IsValid())
	{
		return FText::FromString(BrowserWindow->GetTitle());
	}
	return LOCTEXT("InvalidWindow", "Browser Window is not valid/supported");
}

FString SWebBrowser::GetUrl() const
{
	if (BrowserWindow->IsValid())
	{
		return BrowserWindow->GetUrl();
	}

	return FString();
}

FText SWebBrowser::GetAddressBarUrlText() const
{
	if(BrowserWindow.IsValid())
	{
		return AddressBarUrl;
	}
	return FText::GetEmpty();
}

bool SWebBrowser::IsLoaded() const
{
	if (BrowserWindow.IsValid())
	{
		return (BrowserWindow->GetDocumentLoadingState() == EWebBrowserDocumentState::Completed);
	}

	return false;
}

bool SWebBrowser::IsLoading() const
{
	if (BrowserWindow.IsValid())
	{
		return (BrowserWindow->GetDocumentLoadingState() == EWebBrowserDocumentState::Loading);
	}

	return false;
}

bool SWebBrowser::CanGoBack() const
{
	if (BrowserWindow.IsValid())
	{
		return BrowserWindow->CanGoBack();
	}
	return false;
}

FReply SWebBrowser::OnBackClicked()
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->GoBack();
	}
	return FReply::Handled();
}

bool SWebBrowser::CanGoForward() const
{
	if (BrowserWindow.IsValid())
	{
		return BrowserWindow->CanGoForward();
	}
	return false;
}

FReply SWebBrowser::OnForwardClicked()
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->GoForward();
	}
	return FReply::Handled();
}

FText SWebBrowser::GetReloadButtonText() const
{
	static FText Reload = LOCTEXT("Reload", "Reload");
	static FText Stop = LOCTEXT("Stop", "Stop");

	if (BrowserWindow.IsValid())
	{
		if (BrowserWindow->IsLoading())
		{
			return Stop;
		}
	}
	return Reload;
}

FReply SWebBrowser::OnReloadClicked()
{
	if (BrowserWindow.IsValid())
	{
		if (BrowserWindow->IsLoading())
		{
			BrowserWindow->StopLoad();
		}
		else
		{
			BrowserWindow->Reload();
		}
	}
	return FReply::Handled();
}

void SWebBrowser::OnUrlTextCommitted( const FText& NewText, ETextCommit::Type CommitType )
{
	AddressBarUrl = NewText;
	if(CommitType == ETextCommit::OnEnter)
	{
		LoadURL(AddressBarUrl.ToString());
	}
}

EVisibility SWebBrowser::GetViewportVisibility() const
{
	if (BrowserWindow.IsValid() && BrowserWindow->HasBeenPainted())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

EVisibility SWebBrowser::GetLoadingThrobberVisibility() const
{
	if (BrowserWindow.IsValid() && !BrowserWindow->HasBeenPainted())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

bool SWebBrowser::HandleJSQueryReceived(int64 QueryId, FString QueryString, bool Persistent, FJSQueryResultDelegate Delegate)
{
	if (OnJSQueryReceived.IsBound())
	{
		return OnJSQueryReceived.Execute(QueryId, QueryString, Persistent, Delegate);
	}
	return false;
}

void SWebBrowser::HandleJSQueryCanceled(int64 QueryId)
{
	OnJSQueryCanceled.ExecuteIfBound(QueryId);
}

bool SWebBrowser::HandleBeforeBrowse(FString URL, bool bIsRedirect)
{
	if (OnBeforeBrowse.IsBound())
	{
		return OnBeforeBrowse.Execute(URL, bIsRedirect);
	}

	return false;
}

bool SWebBrowser::HandleBeforePopup(FString URL, FString Target)
{
	if (OnBeforePopup.IsBound())
	{
		return OnBeforePopup.Execute(URL, Target);
	}

	return false;
}

void SWebBrowser::ExecuteJavascript(const FString& JS)
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->ExecuteJavascript(JS);
	}
}

void SWebBrowser::HandleBrowserWindowDocumentStateChanged(EWebBrowserDocumentState NewState)
{
	switch (NewState)
	{
	case EWebBrowserDocumentState::Completed:
		OnLoadCompleted.ExecuteIfBound();
		break;

	case EWebBrowserDocumentState::Error:
		OnLoadError.ExecuteIfBound();
		break;

	case EWebBrowserDocumentState::Loading:
		OnLoadStarted.ExecuteIfBound();
		break;
	}
}

void SWebBrowser::HandleBrowserWindowNeedsRedraw()
{
	if (!IsHandlingRedraw)
	{
		IsHandlingRedraw = true; // Until the delegate has been invoked we don't need to register another timer
		// Tell slate that the widget needs to wake up for one frame to get redrawn
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime) { IsHandlingRedraw = false;  return EActiveTimerReturnType::Stop; }));
	}
}

void SWebBrowser::HandleTitleChanged( FString NewTitle )
{
	const FText NewTitleText = FText::FromString(NewTitle);
	OnTitleChanged.ExecuteIfBound(NewTitleText);
}

void SWebBrowser::HandleUrlChanged( FString NewUrl )
{
	AddressBarUrl = FText::FromString(NewUrl);
	OnTitleChanged.ExecuteIfBound(AddressBarUrl);
}

#undef LOCTEXT_NAMESPACE
