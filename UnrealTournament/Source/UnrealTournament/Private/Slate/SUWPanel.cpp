// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWPanel.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

void SUWPanel::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	PlayerOwner = InPlayerOwner;
	checkSlow(PlayerOwner != NULL);

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);
	ConstructPanel(ViewportSize);
}

void SUWPanel::ConstructPanel(FVector2D ViewportSize){}

void SUWPanel::OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow)
{
	ParentWindow = inParentWindow;
}
void SUWPanel::OnHidePanel()
{
	ParentWindow.Reset();
}

void SUWPanel::ConsoleCommand(FString Command)
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController != NULL)
	{
		PlayerOwner->Exec(PlayerOwner->GetWorld(), *Command, *GLog);
	}
}


TSharedRef<SWidget> SUWPanel::GenerateStringListWidget(TSharedPtr<FString> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(*InItem.Get())
		];
}


#endif