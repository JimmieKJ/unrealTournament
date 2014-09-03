// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "Slate.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsStyle.h"
#include "SUWSystemSettingsDialog.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWCreateGameDialog.h"
#include "SUWControlSettingsDialog.h"
#include "SUWInputBox.h"
#include "SUWMessageBox.h"
#include "SUWScaleBox.h"
#include "UTGameEngine.h"

void SUWindowsDesktop::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	CreateDesktop();
}

void SUWindowsDesktop::CreateDesktop()
{
}


void SUWindowsDesktop::OnMenuOpened()
{
	GameViewportWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}

void SUWindowsDesktop::OnMenuClosed()
{
	FSlateApplication::Get().SetKeyboardFocus(GameViewportWidget);
}

bool SUWindowsDesktop::SupportsKeyboardFocus() const
{
	return true;
}

FReply SUWindowsDesktop::OnKeyboardFocusReceived( const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent )
{
	return FReply::Handled().ReleaseMouseCapture();

}

FReply SUWindowsDesktop::OnKeyUp( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		if (GWorld->GetWorld()->GetMapName().ToLower() != TEXT("ut-entry"))
		{
			CloseMenus();
		}
		return FReply::Handled();
	}
	else
	{
		return FReply::Unhandled();
	}
}

void SUWindowsDesktop::CloseMenus()
{
	if (PlayerOwner != NULL)
	{
		PlayerOwner->HideMenu();
	}
}


FReply SUWindowsDesktop::OnKeyDown( const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent )
{
	return FReply::Handled();
}


FReply SUWindowsDesktop::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Handled();
}

FReply SUWindowsDesktop::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (GWorld->GetWorld()->GetMapName().ToLower() != TEXT("ut-entry"))
	{
		CloseMenus();
	}
	return FReply::Handled();
}

void SUWindowsDesktop::ConsoleCommand(FString Command)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController != NULL)
	{
		PlayerOwner->Exec(PlayerOwner->GetWorld(), *Command, *GLog);
	}
}


FReply SUWindowsDesktop::OnMenuConsoleCommand(FString Command)
{
	ConsoleCommand(Command);
	CloseMenus();
	return FReply::Handled();
}
