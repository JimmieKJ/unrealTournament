
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUWScaleBox.h"
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


	OnJSQueryReceived = InArgs._OnJSQueryReceived;
	OnJSQueryCanceled = InArgs._OnJSQueryCanceled;
	OnBeforeBrowse = InArgs._OnBeforeBrowse;

	SUWPanel::Construct(SUWPanel::FArguments(), InPlayerOwner);
}

void SUTWebBrowserPanel::ConstructPanel(FVector2D ViewportSize)
{
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
				]
			]
		];
	}
}

void SUTWebBrowserPanel::Browse(FString URL)
{
	if (WebBrowserContainer.IsValid())
	{
		WebBrowserContainer->ClearChildren();
		WebBrowserContainer->AddSlot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SAssignNew(WebBrowserPanel, SWebBrowser)
			.InitialURL(URL)
			.ShowControls(ShowControls)
			.ViewportSize(DesiredViewportSize)
			.OnJSQueryReceived(FOnJSQueryReceivedDelegate::CreateRaw(this, &SUTWebBrowserPanel::QueryReceived))
			.OnJSQueryCanceled(FOnJSQueryCanceledDelegate::CreateRaw(this, &SUTWebBrowserPanel::QueryCancelled))
			.OnBeforeBrowse(FOnBeforeBrowseDelegate::CreateRaw(this, &SUTWebBrowserPanel::OnBrowse))
		];
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


void SUTWebBrowserPanel::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	SUWPanel::OnShowPanel(inParentWindow);

	// Temporarily change audio level
	UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
	if (AudioSettings)
	{
		AudioSettings->SetSoundClassVolume(EUTSoundClass::Music, 0);
	}


}
void SUTWebBrowserPanel::OnHidePanel()
{
	SUWPanel::OnHidePanel();
	
	UUTGameUserSettings* UserSettings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());


	// Temporarily change audio level
	UUTAudioSettings* AudioSettings = UUTAudioSettings::StaticClass()->GetDefaultObject<UUTAudioSettings>();
	if (AudioSettings)
	{
		AudioSettings->SetSoundClassVolume(EUTSoundClass::Music, UserSettings->GetSoundClassVolume(EUTSoundClass::Music));
	}
}

bool SUTWebBrowserPanel::QueryReceived( int64 QueryId, FString QueryString, bool Persistent, FJSQueryResultDelegate Delegate )
{
	UE_LOG(UT, Log, TEXT("Javascript %s"), *QueryString);

	if (OnJSQueryReceived.IsBound())
	{
		return OnJSQueryReceived.Execute(QueryId, QueryString, Persistent, Delegate);
	}
	return false;
}

void SUTWebBrowserPanel::QueryCancelled(int64 QueryId)
{
	OnJSQueryCanceled.ExecuteIfBound(QueryId);
}


bool SUTWebBrowserPanel::OnBrowse(FString TargetURL, bool bRedirect)
{
	UE_LOG(UT,Log,TEXT("TargetURL: %s"), *TargetURL);

	if (OnBeforeBrowse.IsBound())
	{
		return OnBeforeBrowse.Execute(TargetURL, bRedirect);
	}

	return false;
/*
	if (TargetURL.Equals(TEXT("http://www.necris.net/fragcenter"),ESearchCase::IgnoreCase))
	{
		return false;	
	}
	else
	{
		return true;
	}
*/	
}



#endif