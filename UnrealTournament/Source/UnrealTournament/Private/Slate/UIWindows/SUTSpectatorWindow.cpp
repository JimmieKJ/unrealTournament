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


FReply SUTSpectatorWindow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
	if (PC && PC->MyUTHUD)
	{
		if ( !PC->bSpectatorMouseChangesView )

		{
			FVector2D MousePosition;
			if (GetGameMousePosition(MousePosition))
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

	return FReply::Unhandled();
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

#endif