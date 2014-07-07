// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "Slate/SUWindowsDesktop.h"
#include "Slate/SUWMessageBox.h"
#include "Slate/SUWindowsStyle.h"
#include "Slate/SUWDialog.h"

UUTLocalPlayer::UUTLocalPlayer(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
}

FString UUTLocalPlayer::GetNickname() const
{
	UUTGameUserSettings* Settings;
	Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	if (Settings) 
	{
		return Settings->GetPlayerName();
	}

	return TEXT("");
}

void UUTLocalPlayer::PlayerAdded(class UGameViewportClient* InViewportClient, int32 InControllerID)
{
	SUWindowsStyle::Initialize();
	Super::PlayerAdded(InViewportClient, InControllerID);
}


void UUTLocalPlayer::ShowMenu()
{
	// Create the slate widget if it doesn't exist
	if (!DesktopSlateWidget.IsValid())
	{
		SAssignNew(DesktopSlateWidget, SUWindowsDesktop).PlayerOwner(this);
		if (DesktopSlateWidget.IsValid())
		{
			GEngine->GameViewport->AddViewportWidgetContent( SNew(SWeakWidget).PossiblyNullContent(DesktopSlateWidget.ToSharedRef()));
		}
	}

	// Make it visible.
	if (DesktopSlateWidget.IsValid())
	{
		// Widget is already valid, just make it visible.
		DesktopSlateWidget->SetVisibility(EVisibility::Visible);
		DesktopSlateWidget->OnMenuOpened();
	}
}
void UUTLocalPlayer::HideMenu()
{
	if (DesktopSlateWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(DesktopSlateWidget.ToSharedRef());
		DesktopSlateWidget->OnMenuClosed();
		DesktopSlateWidget.Reset();
	}

}

void UUTLocalPlayer::ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, const FDialogResultDelegate& Callback)
{
	OpenDialog(
		SNew(SUWMessageBox)
		.PlayerOwner(this)
		.MessageTitle(MessageTitle)
		.MessageText(MessageText)
		.ButtonsMask(Buttons)
		.OnDialogResult(Callback)
		);
}

void UUTLocalPlayer::OpenDialog(TSharedRef<SUWDialog> Dialog)
{
	GEngine->GameViewport->AddViewportWidgetContent(Dialog);
	Dialog->OnDialogOpened();
}

void UUTLocalPlayer::CloseDialog(TSharedRef<SUWDialog> Dialog)
{
	Dialog->OnDialogClosed();
	GEngine->GameViewport->RemoveViewportWidgetContent(Dialog);
}