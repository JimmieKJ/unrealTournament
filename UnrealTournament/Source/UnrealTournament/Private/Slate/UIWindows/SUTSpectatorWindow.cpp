// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTMenuBase.h"
#include "SUTSpectatorWindow.h"
#include "UTHUDWidget_SpectatorSlideOut.h"
#include "UTGameViewportClient.h"

#if !UE_SERVER

// @Returns true if the mouse position is inside the viewport
bool SUTSpectatorWindow::GetGameMousePosition(FVector2D& MousePosition)
{
	// We need to get the mouse input but the mouse event only has the mouse in screen space.  We need it in viewport space and there
	// isn't a good way to get there.  So we punt and just get it from the game viewport.

	UUTGameViewportClient* GVC = Cast<UUTGameViewportClient>(PlayerOwner->ViewportClient);
	if (GVC)
	{
		return GVC->GetMousePosition(MousePosition);
	}
	return false;
}


FReply SUTSpectatorWindow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		if ( !PC->bSpectatorMouseChangesView )
		{
			FVector2D MousePosition;
			if ( GetGameMousePosition(MousePosition) )
			{
				UUTHUDWidget_SpectatorSlideOut* SpectatorWidget = PC->MyUTHUD->GetSpectatorSlideOut();
				if (SpectatorWidget)
				{
					SpectatorWidget->SetMouseInteractive(true);
					if ( SpectatorWidget->MouseClick(MousePosition) )
					{
						return FReply::Handled();
					}

					if (!PC->bSpectatorMouseChangesView)
					{
						PC->SetSpectatorMouseChangesView(true);
					}
				}
			}
		}
	}

	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
	return FReply::Handled();
}


FReply SUTSpectatorWindow::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		FVector2D MousePosition;
		if (!PC->bSpectatorMouseChangesView && GetGameMousePosition(MousePosition))
		{
			UUTHUDWidget_SpectatorSlideOut* SpectatorWidget = PC->MyUTHUD->GetSpectatorSlideOut();
			if (SpectatorWidget)
			{
				SpectatorWidget->SetMouseInteractive(true);
				SpectatorWidget->TrackMouseMovement(MousePosition);
			}
		}
	}

	return FReply::Unhandled();
}

void SUTSpectatorWindow::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		if (PC->bSpectatorMouseChangesView != bLast)
		{
			bLast = PC->bSpectatorMouseChangesView;
			FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
		}
	}

}

bool SUTSpectatorWindow::SupportsKeyboardFocus() const
{
	return true;
}

FReply SUTSpectatorWindow::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && PlayerOwner.IsValid())
	{
		PlayerOwner->ShowMenu(TEXT(""));
		return FReply::Handled();
	}
	else if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->InputKey(InKeyEvent.GetKey(), EInputEvent::IE_Released, 1.f, false))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SUTSpectatorWindow::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController && PlayerOwner->PlayerController->InputKey(InKeyEvent.GetKey(), EInputEvent::IE_Pressed, 1.f, false))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

#endif