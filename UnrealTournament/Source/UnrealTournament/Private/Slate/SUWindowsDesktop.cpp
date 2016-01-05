// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"
#include "SUWindowsStyle.h"
#include "Dialogs/SUTSystemSettingsDialog.h"
#include "Dialogs/SUTPlayerSettingsDialog.h"
#include "Dialogs/SUTControlSettingsDialog.h"
#include "Dialogs/SUTInputBoxDialog.h"
#include "Dialogs/SUTMessageBoxDialog.h"
#include "Widgets/SUTScaleBox.h"
#include "Base/SUTPanelBase.h"
#include "UTGameEngine.h"
#include "Runtime/Engine/Classes/Engine/Console.h"
#include "SOverlay.h"

#if !UE_SERVER
void SUWindowsDesktop::Construct(const FArguments& InArgs)
{
	ZOrderIndex = -1;
	PlayerOwner = InArgs._PlayerOwner;
	CreateDesktop();
}

void SUWindowsDesktop::CreateDesktop()
{
}


void SUWindowsDesktop::OnMenuOpened(const FString& Parameters)
{
	GameViewportWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);

	if (!PlayerOnlineStatusChangedDelegate.IsValid())
	{
		PlayerOnlineStatusChangedDelegate = PlayerOwner->RegisterPlayerOnlineStatusChangedDelegate(FPlayerOnlineStatusChanged::FDelegate::CreateSP(this, &SUWindowsDesktop::OwnerLoginStatusChanged));
	}
}

void SUWindowsDesktop::OnMenuClosed()
{
	// Deactivate the current panel

	if (ActivePanel.IsValid())
	{
		DeactivatePanel(ActivePanel);
	}

	FSlateApplication::Get().ClearUserFocus(0);
	FSlateApplication::Get().ClearKeyboardFocus();

	PlayerOwner->RemovePlayerOnlineStatusChangedDelegate(PlayerOnlineStatusChangedDelegate);
	PlayerOnlineStatusChangedDelegate.Reset();
}

void SUWindowsDesktop::OnOwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
}

void SUWindowsDesktop::OwnerLoginStatusChanged(UUTLocalPlayer* LocalPlayerOwner, ELoginStatus::Type NewStatus, const FUniqueNetId& UniqueID)
{
	OnOwnerLoginStatusChanged(LocalPlayerOwner, NewStatus, UniqueID);
}

bool SUWindowsDesktop::SupportsKeyboardFocus() const
{
	return true;
}

FReply SUWindowsDesktop::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent )
{
	return FReply::Handled().ReleaseMouseCapture();

}

FReply SUWindowsDesktop::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent )
{
	if (InKeyboardEvent.GetKey() == EKeys::Escape)
	{
		if (GWorld->GetWorld()->GetMapName().ToLower() != TEXT("ut-entry"))
		{
			CloseMenus();
		}
	}
	else if (InKeyboardEvent.GetKey() == EKeys::F9)
	{
		ConsoleCommand(TEXT("SHOT SHOWUI"));
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

FReply SUWindowsDesktop::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent)
{
	if ( GetDefault<UInputSettings>()->ConsoleKeys.Contains(InKeyboardEvent.GetKey()) )
	{
		PlayerOwner->ViewportClient->ViewportConsole->FakeGotoState(FName(TEXT("Open")));
	}

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


FReply SUWindowsDesktop::OnMenuConsoleCommand(FString Command)
{
	ConsoleCommand(Command);
	CloseMenus();
	return FReply::Handled();
}

void SUWindowsDesktop::ActivatePanel(TSharedPtr<class SUTPanelBase> PanelToActivate)
{
	if ( !Desktop.IsValid() ) return;		// Quick out if no place to put it
	
	// Don't reactivate the current panel
	if (ActivePanel != PanelToActivate)
	{
		// Deactive the current panel.
		if ( ActivePanel.IsValid() )
		{
			DeactivatePanel(ActivePanel);
		}	
	
		ZOrderIndex = (ZOrderIndex + 1) % 5000;

		// Add the new one.
		SOverlay::FOverlaySlot& Slot = Desktop->AddSlot(ZOrderIndex)
		[
			PanelToActivate.ToSharedRef()
		];
		PanelToActivate->ZOrder = ZOrderIndex;
		ActivePanel = PanelToActivate;
		ActivePanel->OnShowPanel(SharedThis(this));
	}
}

void SUWindowsDesktop::DeactivatePanel(TSharedPtr<class SUTPanelBase> PanelToDeactivate)
{
	PanelToDeactivate->OnHidePanel();
}

void SUWindowsDesktop::PanelHidden(TSharedPtr<SWidget> Child)
{
	if (Child.IsValid())
	{
		// SO TOTALLY Unsafe.. 
		TSharedPtr<SUTPanelBase> const Panel = StaticCastSharedPtr<SUTPanelBase>(Child);
		Desktop->RemoveSlot(Panel->ZOrder);
		if (Child == ActivePanel)
		{
			ActivePanel.Reset();
		}
	}
}

void SUWindowsDesktop::ShowHomePanel()
{
}


#endif