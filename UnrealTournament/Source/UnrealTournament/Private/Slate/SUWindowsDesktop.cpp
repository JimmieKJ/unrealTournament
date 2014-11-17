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
#include "SUWPanel.h"
#include "UTGameEngine.h"

#if !UE_SERVER
void SUWindowsDesktop::Construct(const FArguments& InArgs)
{
	PlayerOwner = InArgs._PlayerOwner;
	DesktopSlotIndex = -1;
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
	TSharedPtr<SViewport> VP = StaticCastSharedPtr<SViewport>(GameViewportWidget);
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
	}
	return FReply::Handled();
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

FReply SUWindowsDesktop::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return FReply::Handled();
}


FReply SUWindowsDesktop::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Handled();
}

FReply SUWindowsDesktop::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return FReply::Handled();
}

void SUWindowsDesktop::ConsoleCommand(FString Command)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController != NULL)
	{
		PlayerOwner->Exec(PlayerOwner->GetWorld(), *Command, *GLog);
	}
}


FReply SUWindowsDesktop::OnMenuConsoleCommand(FString Command, TSharedPtr<SComboButton> MenuButton)
{
	if (MenuButton.IsValid()) MenuButton->SetIsOpen(false);
	ConsoleCommand(Command);
	CloseMenus();
	return FReply::Handled();
}

void SUWindowsDesktop::ActivatePanel(TSharedPtr<class SUWPanel> PanelToActivate)
{
	if ( !Desktop.IsValid() ) return;		// Quick out if no place to put it
	// Don't reactivate the current panel
	if (ActivePanel != PanelToActivate)
	{
		if ( ActivePanel.IsValid() )
		{
			DeactivatePanel(ActivePanel.ToSharedRef());
		}	

		SOverlay::FOverlaySlot& Slot = Desktop->AddSlot()
			[
				PanelToActivate.ToSharedRef()
			];
		
		DesktopSlotIndex = Slot.ZOrder;
		ActivePanel = PanelToActivate;
		ActivePanel->OnShowPanel(MakeShareable(this));
	}
}

void SUWindowsDesktop::DeactivatePanel(TSharedPtr<class SUWPanel> PanelToDeactivate)
{
	ActivePanel->OnHidePanel();
	Desktop->ClearChildren();

}
#endif