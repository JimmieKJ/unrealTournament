// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WebBrowserPrivatePCH.h"
#include "SWebBrowser.h"
#include "SThrobber.h"
#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"
#include "IWebBrowserWindow.h"
#include "WebBrowserViewport.h"
#include "IWebBrowserAdapter.h"

#define LOCTEXT_NAMESPACE "WebBrowser"

SWebBrowser::SWebBrowser()
{
}

SWebBrowser::~SWebBrowser()
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->OnCreateWindow().Unbind();
		BrowserWindow->OnCloseWindow().Unbind();
		BrowserWindow->OnDocumentStateChanged().RemoveAll(this);
		BrowserWindow->OnNeedsRedraw().RemoveAll(this);
		BrowserWindow->OnTitleChanged().RemoveAll(this);
		BrowserWindow->OnUrlChanged().RemoveAll(this);
		BrowserWindow->OnShowPopup().RemoveAll(this);
		BrowserWindow->OnDismissPopup().RemoveAll(this);
		BrowserWindow->OnBeforeBrowse().Unbind();
		BrowserWindow->OnLoadUrl().Unbind();
		BrowserWindow->OnShowDialog().Unbind();
		BrowserWindow->OnDismissAllDialogs().Unbind();
		BrowserWindow->OnBeforePopup().Unbind();
		if(!BrowserWindow->IsClosing())
		{
			BrowserWindow->CloseBrowser(true);
		}
	}
}

void SWebBrowser::Construct(const FArguments& InArgs, const TSharedPtr<IWebBrowserWindow>& InWebBrowserWindow)
{
	OnLoadCompleted = InArgs._OnLoadCompleted;
	OnLoadError = InArgs._OnLoadError;
	OnLoadStarted = InArgs._OnLoadStarted;
	OnTitleChanged = InArgs._OnTitleChanged;
	OnUrlChanged = InArgs._OnUrlChanged;
	OnBeforeNavigation = InArgs._OnBeforeNavigation;
	OnLoadUrl = InArgs._OnLoadUrl;
	OnShowDialog = InArgs._OnShowDialog;
	OnDismissAllDialogs = InArgs._OnDismissAllDialogs;
	OnBeforePopup = InArgs._OnBeforePopup;
	OnJSQueryReceived = InArgs._OnJSQueryReceived;
	OnJSQueryCanceled = InArgs._OnJSQueryCanceled;
	OnCreateWindow = InArgs._OnCreateWindow;
	OnCloseWindow = InArgs._OnCloseWindow;
	AddressBarUrl = FText::FromString(InArgs._InitialURL);
	PopupMenuMethod = InArgs._PopupMenuMethod;
	bShowInitialThrobber = InArgs._ShowInitialThrobber;

	BrowserWindow = InWebBrowserWindow;
	if(!BrowserWindow.IsValid())
	{
		void* OSWindowHandle = nullptr;
		if (InArgs._ParentWindow.IsValid())
		{
			TSharedPtr<FGenericWindow> NativeWindow = InArgs._ParentWindow->GetNativeWindow();
			OSWindowHandle = NativeWindow->GetOSWindowHandle();
		}

		BrowserWindow = IWebBrowserModule::Get().GetSingleton()->CreateBrowserWindow(
			OSWindowHandle,
			InArgs._InitialURL,
			InArgs._SupportsTransparency,
			InArgs._SupportsThumbMouseButtonNavigation,
			InArgs._ContentsToLoad,
			InArgs._ShowErrorMessage,
			InArgs._BackgroundColor
			);
	}

	TSharedRef<SWidget> BrowserWidgetRef = BrowserWindow.IsValid() ? BrowserWindow->CreateWidget(InArgs._ViewportSize) : SNullWidget::NullWidget;
	BrowserWidgetRef->SetVisibility(TAttribute<EVisibility>(this, &SWebBrowser::GetViewportVisibility));
	BrowserWidget = BrowserWidgetRef;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.Visibility((InArgs._ShowControls || InArgs._ShowAddressBar) ? EVisibility::Visible : EVisibility::Collapsed)
			+ SHorizontalBox::Slot()
			.Padding(0, 5)
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
			.Padding(5.f, 5.f)
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
			+ SOverlay::Slot()
			[
				BrowserWidgetRef
			]
			+ SOverlay::Slot()
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
		if(OnCreateWindow.IsBound())
		{
			BrowserWindow->OnCreateWindow().BindSP(this, &SWebBrowser::HandleCreateWindow);
		}

		if(OnCloseWindow.IsBound())
		{
			BrowserWindow->OnCloseWindow().BindSP(this, &SWebBrowser::HandleCloseWindow);
		}

		BrowserWindow->OnDocumentStateChanged().AddSP(this, &SWebBrowser::HandleBrowserWindowDocumentStateChanged);
		BrowserWindow->OnNeedsRedraw().AddSP(this, &SWebBrowser::HandleBrowserWindowNeedsRedraw);
		BrowserWindow->OnTitleChanged().AddSP(this, &SWebBrowser::HandleTitleChanged);
		BrowserWindow->OnUrlChanged().AddSP(this, &SWebBrowser::HandleUrlChanged);
		BrowserWindow->OnBeforeBrowse().BindSP(this, &SWebBrowser::HandleBeforeNavigation);
		BrowserWindow->OnLoadUrl().BindSP(this, &SWebBrowser::HandleLoadUrl);
		BrowserWindow->OnShowDialog().BindSP(this, &SWebBrowser::HandleShowDialog);
		BrowserWindow->OnDismissAllDialogs().BindSP(this, &SWebBrowser::HandleDismissAllDialogs);
		BrowserWindow->OnBeforePopup().BindSP(this, &SWebBrowser::HandleBeforePopup);
		BrowserWindow->OnShowPopup().AddSP(this, &SWebBrowser::HandleShowPopup);
		BrowserWindow->OnDismissPopup().AddSP(this, &SWebBrowser::HandleDismissPopup);
	}
	else
	{
		OnLoadError.ExecuteIfBound();
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

void SWebBrowser::Reload()
{
	if (BrowserWindow.IsValid())
	{
	BrowserWindow->Reload();
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
	if (BrowserWindow.IsValid())
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

void SWebBrowser::GoBack()
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->GoBack();
	}
}

FReply SWebBrowser::OnBackClicked()
{
	GoBack();
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

void SWebBrowser::GoForward()
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->GoForward();
	}
}

FReply SWebBrowser::OnForwardClicked()
{
	GoForward();
	return FReply::Handled();
}

FText SWebBrowser::GetReloadButtonText() const
{
	static FText ReloadText = LOCTEXT("Reload", "Reload");
	static FText StopText = LOCTEXT("StopText", "Stop");

	if (BrowserWindow.IsValid())
	{
		if (BrowserWindow->IsLoading())
		{
			return StopText;
		}
	}
	return ReloadText;
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
			Reload();
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
	if (!bShowInitialThrobber || (BrowserWindow.IsValid() &&  BrowserWindow->IsInitialized()))
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

EVisibility SWebBrowser::GetLoadingThrobberVisibility() const
{
	if (bShowInitialThrobber && (!BrowserWindow.IsValid() || !BrowserWindow->IsInitialized()))
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

void SWebBrowser::HandleBrowserWindowDocumentStateChanged(EWebBrowserDocumentState NewState)
{
	switch (NewState)
	{
	case EWebBrowserDocumentState::Completed:
		{
			if (BrowserWindow.IsValid())
			{
				for (auto Adapter : Adapters)
				{
					Adapter->ConnectTo(BrowserWindow.ToSharedRef());
				}
			}

			OnLoadCompleted.ExecuteIfBound();
		}
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
	if (FSlateApplication::Get().IsSlateAsleep())
	{
		// Tell slate that the widget needs to wake up for one frame to get redrawn
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime) { return EActiveTimerReturnType::Stop; }));
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
	OnUrlChanged.ExecuteIfBound(AddressBarUrl);
}

bool SWebBrowser::HandleBeforeNavigation(const FString& Url, bool bIsRedirect)
{
	if(OnBeforeNavigation.IsBound())
	{
		return OnBeforeNavigation.Execute(Url, bIsRedirect);
	}
	return false;
}

bool SWebBrowser::HandleLoadUrl(const FString& Method, const FString& Url, FString& OutResponse)
{
	if(OnLoadUrl.IsBound())
	{
		return OnLoadUrl.Execute(Method, Url, OutResponse);
	}
	return false;
}

EWebBrowserDialogEventResponse SWebBrowser::HandleShowDialog(const TWeakPtr<IWebBrowserDialog>& DialogParams)
{
	if(OnShowDialog.IsBound())
	{
		return OnShowDialog.Execute(DialogParams);
	}
	return EWebBrowserDialogEventResponse::Unhandled;
}

void SWebBrowser::HandleDismissAllDialogs()
{
	OnDismissAllDialogs.ExecuteIfBound();
}


bool SWebBrowser::HandleBeforePopup(FString URL, FString Target)
{
	if (OnBeforePopup.IsBound())
	{
		return OnBeforePopup.Execute(URL, Target);
	}

	return false;
}

void SWebBrowser::ExecuteJavascript(const FString& ScriptText)
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->ExecuteJavascript(ScriptText);
	}
}

bool SWebBrowser::HandleCreateWindow(const TWeakPtr<IWebBrowserWindow>& NewBrowserWindow, const TWeakPtr<IWebBrowserPopupFeatures>& PopupFeatures)
{
	if(OnCreateWindow.IsBound())
	{
		return OnCreateWindow.Execute(NewBrowserWindow, PopupFeatures);
	}
	return false;
}

bool SWebBrowser::HandleCloseWindow(const TWeakPtr<IWebBrowserWindow>& NewBrowserWindow)
{
	if(OnCloseWindow.IsBound())
	{
		return OnCloseWindow.Execute(NewBrowserWindow);
	}
	return false;
}

void SWebBrowser::BindUObject(const FString& Name, UObject* Object, bool bIsPermanent)
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->BindUObject(Name, Object, bIsPermanent);
	}
}

void SWebBrowser::UnbindUObject(const FString& Name, UObject* Object, bool bIsPermanent)
{
	if (BrowserWindow.IsValid())
	{
		BrowserWindow->UnbindUObject(Name, Object, bIsPermanent);
	}
}

void SWebBrowser::BindAdapter(const TSharedRef<IWebBrowserAdapter>& Adapter)
{
	Adapters.Add(Adapter);
	if (BrowserWindow.IsValid())
	{
		Adapter->ConnectTo(BrowserWindow.ToSharedRef());
	}
}

void SWebBrowser::UnbindAdapter(const TSharedRef<IWebBrowserAdapter>& Adapter)
{
	Adapters.Remove(Adapter);
	if (BrowserWindow.IsValid())
	{
		Adapter->DisconnectFrom(BrowserWindow.ToSharedRef());
	}
}

void SWebBrowser::HandleShowPopup(const FIntRect& PopupSize)
{
	check(!PopupMenuPtr.IsValid())

	TSharedPtr<SViewport> MenuContent;
	SAssignNew(MenuContent, SViewport)
				.ViewportSize(PopupSize.Size())
				.EnableGammaCorrection(false)
				.EnableBlending(false)
				.IgnoreTextureAlpha(true)
				.Visibility(EVisibility::Visible);
	MenuViewport = MakeShareable(new FWebBrowserViewport(BrowserWindow, true));
	MenuContent->SetViewportInterface(MenuViewport.ToSharedRef());
	FWidgetPath WidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetUnchecked(BrowserWidget.ToSharedRef(), WidgetPath);
	if (WidgetPath.IsValid())
	{
		TSharedRef< SWidget > MenuContentRef = MenuContent.ToSharedRef();
		const FGeometry& BrowserGeometry = WidgetPath.Widgets.Last().Geometry;
		const FVector2D NewPosition = BrowserGeometry.LocalToAbsolute(PopupSize.Min);


		// Open the pop-up. The popup method will be queried from the widget path passed in.
		TSharedPtr<IMenu> NewMenu = FSlateApplication::Get().PushMenu(BrowserWidget.ToSharedRef(), WidgetPath, MenuContentRef, NewPosition, FPopupTransitionEffect( FPopupTransitionEffect::ComboButton ), false);
		NewMenu->GetOnMenuDismissed().AddSP(this, &SWebBrowser::HandleMenuDismissed);
		PopupMenuPtr = NewMenu;
	}

}

void SWebBrowser::HandleMenuDismissed(TSharedRef<IMenu>)
{
	PopupMenuPtr.Reset();
}


FPopupMethodReply SWebBrowser::OnQueryPopupMethod() const
{
	return PopupMenuMethod.IsSet()
		? FPopupMethodReply::UseMethod(PopupMenuMethod.GetValue())
		: FPopupMethodReply::Unhandled();
}

void SWebBrowser::HandleDismissPopup()
{
	if (PopupMenuPtr.IsValid())
	{
		PopupMenuPtr.Pin()->Dismiss();
		FSlateApplication::Get().SetKeyboardFocus(BrowserWidget, EFocusCause::SetDirectly);
	}
}


#undef LOCTEXT_NAMESPACE

