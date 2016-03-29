
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../Widgets/SUTScaleBox.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "SUTWebBrowserPanel.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

void SUTWebBrowserPanel::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	ShowControls = InArgs._ShowControls;
	DesiredViewportSize = InArgs._ViewportSize;
	bAllowScaling = InArgs._AllowScaling;
	
	InitialURL = InArgs._InitialURL;
	OnBeforeBrowse = InArgs._OnBeforeBrowse;
	OnBeforePopup = InArgs._OnBeforePopup;

	SUTPanelBase::Construct(SUTPanelBase::FArguments(), InPlayerOwner);
}

void SUTWebBrowserPanel::ConstructPanel(FVector2D ViewportSize)
{
	bShowWarning = false;

	if (bAllowScaling)
	{
		this->ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(Overlay, SOverlay)
			+SOverlay::Slot()
			[
				SAssignNew(WebBrowserContainer, SVerticalBox)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(WebBrowserPanel, SWebBrowser)
					.InitialURL(InitialURL)
					.ShowControls(ShowControls)
					.ViewportSize(DesiredViewportSize)
					.OnBeforeNavigation(SWebBrowser::FOnBeforeBrowse::CreateSP(this, &SUTWebBrowserPanel::BeforeBrowse))
					.OnBeforePopup(FOnBeforePopupDelegate::CreateSP(this, &SUTWebBrowserPanel::BeforePopup))
				]

			]
		];
	}
	else
	{
		this->ChildSlot
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SDPIScaler)
			.DPIScale(this, &SUTWebBrowserPanel::GetReverseScale)
			[
				SAssignNew(Overlay, SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(WebBrowserContainer, SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Fill)
					.HAlign(HAlign_Fill)
					[
						SAssignNew(WebBrowserPanel, SWebBrowser)
						.InitialURL(InitialURL)
						.ShowControls(ShowControls)
						.ViewportSize(DesiredViewportSize)
						.OnBeforeNavigation(SWebBrowser::FOnBeforeBrowse::CreateSP(this, &SUTWebBrowserPanel::BeforeBrowse))
						.OnBeforePopup(FOnBeforePopupDelegate::CreateSP(this, &SUTWebBrowserPanel::BeforePopup))
					]
				]
			]
		];
	}
}

void SUTWebBrowserPanel::Browse(FString URL)
{
	if (WebBrowserPanel.IsValid())
	{
		WebBrowserPanel->LoadURL(URL);
	}
}


float SUTWebBrowserPanel::GetReverseScale() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->ViewportClient)
	{
		FVector2D ViewportSize;
		PlayerOwner->ViewportClient->GetViewportSize(ViewportSize);
		return 1.0f / GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass())->GetDPIScaleBasedOnSize(FIntPoint(ViewportSize.X, ViewportSize.Y));
	}
	return 1.0f;
}


void SUTWebBrowserPanel::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	SUTPanelBase::OnShowPanel(inParentWindow);

	// Temporarily change audio level
	UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
	if (AudioSettings)
	{
		AudioSettings->SetSoundClassVolume(EUTSoundClass::Music, 0);
	}


}
void SUTWebBrowserPanel::OnHidePanel()
{
	SUTPanelBase::OnHidePanel();
	
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());


	// Temporarily change audio level
	UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
	if (AudioSettings)
	{
		AudioSettings->SetSoundClassVolume(EUTSoundClass::Music, UserSettings->GetSoundClassVolume(EUTSoundClass::Music));
	}
}

bool SUTWebBrowserPanel::BeforeBrowse(const FString& TargetURL, const FWebNavigationRequest& WebRequest)
{
	if (OnBeforeBrowse.IsBound())
	{
		return OnBeforeBrowse.Execute(TargetURL, WebRequest);
	}
	
	return false;
}

bool SUTWebBrowserPanel::BeforePopup(FString TargetURL, FString FrameName)
{
	if (OnBeforePopup.IsBound())
	{
		return OnBeforePopup.Execute(TargetURL, FrameName);
	}

	DesiredURL = TargetURL;
	// These events happen on the render thread.  So we have to stall and wait for the game thread otherwise
	// slate will "crash" via assert.
	bShowWarning = true;	
	return true;
}

void SUTWebBrowserPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bShowWarning)
	{
		bShowWarning = false;	
		GetPlayerOwner()->ShowMessage(NSLOCTEXT("SUTFragCenterPanel", "ExternalURLTitle", "WARNING External URL"), NSLOCTEXT("SUTFragCenterPanel", "ExternalURLMessage", "The URL you have selected is outside of the Unreal network and may be dangerous.  Are you sure you wish to go there?"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUTWebBrowserPanel::WarningResult));
	}
}


void SUTWebBrowserPanel::WarningResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		FPlatformProcess::LaunchURL(*DesiredURL, NULL, NULL);
	}
}


#endif