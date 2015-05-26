
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUWScaleBox.h"
#include "SlateExtras.h"
#include "Slate/SlateGameResources.h"
#include "SUTFragCenterPanel.h"
#include "Engine/UserInterfaceSettings.h"

#if !UE_SERVER

void SUTFragCenterPanel::ConstructPanel(FVector2D ViewportSize)
{
	SUTWebBrowserPanel::ConstructPanel(ViewportSize);

	bShowWarning = false;
	if (Overlay.IsValid())
	{
		Overlay->AddSlot(200)
			.Padding(1350, 735, 0, 0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(500)
						.HeightOverride(32)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Right)
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SAssignNew(AutoMuteToggleButton, SUTButton)
									.OnClicked(this, &SUTFragCenterPanel::ChangeAutoMute)
									.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
									[
										SNew(STextBlock)
										.Text( TAttribute<FText>::Create( TAttribute<FText>::FGetter::CreateSP( this, &SUTFragCenterPanel::GetAutoMuteText)))
										.TextStyle(SUWindowsStyle::Get(), "UT.Hub.RulesText_Small")
										.ColorAndOpacity(FLinearColor::Black)
									]
								]

								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SAssignNew(AutoPlayToggleButton, SUTButton)
									.OnClicked(this, &SUTFragCenterPanel::ChangeAutoPlay)
									.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.MenuButton")
									[
										SNew(STextBlock)
										.Text( TAttribute<FText>::Create( TAttribute<FText>::FGetter::CreateSP( this, &SUTFragCenterPanel::GetAutoPlayText)))
										.TextStyle(SUWindowsStyle::Get(), "UT.Hub.RulesText_Small")
										.ColorAndOpacity(FLinearColor::Black)
									]
								]

							]
						]
					]
				]
			];
	}
}

FText SUTFragCenterPanel::GetAutoPlayText()
{
	return GetPlayerOwner()->bFragCenterAutoPlay ? NSLOCTEXT("SUTFragCenterPanel","AutoPlayOn","[AutoPlay: ON]") : NSLOCTEXT("SUTFragCenterPanel","AutoPlayOff","[AutoPlay: OFF]");
}

FText SUTFragCenterPanel::GetAutoMuteText()
{
	return GetPlayerOwner()->bFragCenterAutoMute ? NSLOCTEXT("SUTFragCenterPanel","AutoMuteOn","[Auto-Mute: ON]") : NSLOCTEXT("SUTFragCenterPanel","AutoMuteOff","[Auto-Mute: OFF]");
}


FReply SUTFragCenterPanel::ChangeAutoPlay()
{
	GetPlayerOwner()->bFragCenterAutoPlay = !GetPlayerOwner()->bFragCenterAutoPlay;
	GetPlayerOwner()->SaveConfig();

	FString JavaCommand = FString::Printf( TEXT("setAutoStart(%s);"), (GetPlayerOwner()->bFragCenterAutoPlay ? TEXT("true") : TEXT("false")) );
	ExecuteJavascript(JavaCommand);

	return FReply::Handled();	
}

FReply SUTFragCenterPanel::ChangeAutoMute()
{
	GetPlayerOwner()->bFragCenterAutoMute = !GetPlayerOwner()->bFragCenterAutoMute;
	GetPlayerOwner()->SaveConfig();

	FString JavaCommand = FString::Printf( TEXT("setAutoMute(%s);"), (GetPlayerOwner()->bFragCenterAutoMute ? TEXT("true") : TEXT("false")) );
	ExecuteJavascript(JavaCommand);


	return FReply::Handled();	
}

bool SUTFragCenterPanel::BeforePopup(FString URL, FString Target)
{
	DesiredURL = URL;
	// These events happen on the render thread.  So we have to stall and wait for the game thread otherwise
	// slate will "crash" via assert.
	bShowWarning = true;	
	return true;
}

void SUTFragCenterPanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SUTWebBrowserPanel::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (bShowWarning)
	{
		bShowWarning = false;	
		GetPlayerOwner()->ShowMessage(NSLOCTEXT("SUTFragCenterPanel", "ExternalURLTitle", "WARNING External URL"), NSLOCTEXT("SUTFragCenterPanel", "ExternalURLMessage", "The URL you have selected is outside of the Unreal network and may be dangerous.  Are you sure you wish to go there?"), UTDIALOG_BUTTON_YES + UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUTFragCenterPanel::WarningResult));
	}
}


void SUTFragCenterPanel::WarningResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		FPlatformProcess::LaunchURL(*DesiredURL, NULL, NULL);
	}
}

bool SUTFragCenterPanel::QueryReceived( int64 QueryId, FString QueryString, bool Persistent, FJSQueryResultDelegate Delegate )
{

	if (QueryString == TEXT("player_active"))
	{
		FString JavaCommand = FString::Printf( TEXT("setAutoStart(%s);"), (GetPlayerOwner()->bFragCenterAutoPlay ? TEXT("true") : TEXT("false")) );
		ExecuteJavascript(JavaCommand);

		JavaCommand = FString::Printf( TEXT("setAutoMute(%s);"), (GetPlayerOwner()->bFragCenterAutoMute ? TEXT("true") : TEXT("false")) );
		ExecuteJavascript(JavaCommand);
	}

	return SUTWebBrowserPanel::QueryReceived(QueryId, QueryString, Persistent, Delegate);

}

#endif