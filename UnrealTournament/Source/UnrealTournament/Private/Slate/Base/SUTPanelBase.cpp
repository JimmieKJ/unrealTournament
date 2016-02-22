// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTPanelBase.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTPanelBase::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	bClosing = false;
	PlayerOwner = InPlayerOwner;
	checkSlow(PlayerOwner != NULL);

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);
	ConstructPanel(ViewportSize);
}

void SUTPanelBase::ConstructPanel(FVector2D ViewportSize){}

void SUTPanelBase::OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow)
{
	ParentWindow = inParentWindow;
}
void SUTPanelBase::OnHidePanel()
{
	TSharedPtr<SWidget> Panel = this->AsShared();
	bClosing = true;
	ParentWindow->PanelHidden(Panel);
	ParentWindow.Reset();
}

void SUTPanelBase::ConsoleCommand(FString Command)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController != NULL)
	{
		PlayerOwner->Exec(PlayerOwner->GetWorld(), *Command, *GLog);
	}
}


TSharedRef<SWidget> SUTPanelBase::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(FText::FromString(*InItem.Get()))
		];
}


AUTPlayerState* SUTPanelBase::GetOwnerPlayerState()
{
	AUTBasePlayerController* PC = Cast<AUTBasePlayerController>(PlayerOwner->PlayerController);
	if (PC) 
	{
		return Cast<AUTPlayerState>(PC->PlayerState);
	}
	return NULL;
}

TSharedPtr<SWidget> SUTPanelBase::GetInitialFocus()
{
	return nullptr;
}


#endif