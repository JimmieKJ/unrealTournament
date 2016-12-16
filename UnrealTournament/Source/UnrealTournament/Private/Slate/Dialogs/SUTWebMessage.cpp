// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "UTProfileSettings.h"
#include "SUTWebMessage.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"
#include "../SUTUtils.h"

#if !UE_SERVER


void SUTWebMessage::Construct(const FArguments& InArgs)
{
	AUTPlayerController* PC = Cast<AUTPlayerController>(InArgs._PlayerOwner->PlayerController);
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.IsScrollable(false)
							.bShadow(true) 
							.OnDialogResult(InArgs._OnDialogResult)
						);

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[
			SAssignNew(WebBrowser, SUTWebBrowserPanel, PlayerOwner)
			.InitialURL(TEXT("http://epic.gm/utcontrib"))
			.ShowControls(false)
		];
	}

}


void SUTWebMessage::Browse(FText Caption, FString Url)
{
	DialogTitle->SetText(Caption);
	WebBrowser->Browse(Url);
}


FReply SUTWebMessage::OnButtonClick(uint16 ButtonID)
{
	PlayerOwner->CloseWebMessage();
	return FReply::Handled();
}

#endif