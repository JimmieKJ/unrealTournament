// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "Slate/SUWindowsDesktop.h"
#include "Slate/SUWMessageBox.h"
#include "Slate/SUWindowsStyle.h"


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

void UUTLocalPlayer::ShowMessage(FText MessageTitle, FText MessageText, uint16 Buttons, UObject* Host, FName ResultFunction)
{
	// Make sure we don't have a message box up.  Right now we only support 1 messasge box.   In time maybe we will support more.
	if (!MessageBoxWidget.IsValid())
	{
		SAssignNew(MessageBoxWidget, SUWMessageBox)
			.PlayerOwner(this)
			.MessageTitle(MessageTitle)
			.MessageText(MessageText)
			.ButtonsMask(Buttons);

		if (MessageBoxWidget.IsValid())
		{
			MessageBoxWidget->OnDialogOpened();
			if (ResultFunction != NAME_None)
			{
				OnDialogResult.BindUFunction(Host, ResultFunction);
			}
			GEngine->GameViewport->AddViewportWidgetContent( SNew(SWeakWidget).PossiblyNullContent(MessageBoxWidget.ToSharedRef()));
		}
	}
	else
	{
		UE_LOG(UT,Log,TEXT("Attempting to Open A second MessageBox with Title [%s] and Text [%s]"), *MessageTitle.ToString(), *MessageText.ToString());
	}
}

void UUTLocalPlayer::MessageBoxDialogResult(uint16 ButtonID)
{
	OnDialogResult.ExecuteIfBound(ButtonID);
	
	if (MessageBoxWidget.IsValid())
	{
		MessageBoxWidget->OnDialogClosed();
		GEngine->GameViewport->RemoveViewportWidgetContent(MessageBoxWidget.ToSharedRef());
		MessageBoxWidget = NULL;
	}
}