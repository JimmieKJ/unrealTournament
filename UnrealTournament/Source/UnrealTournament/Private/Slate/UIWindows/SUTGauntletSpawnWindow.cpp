// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTGauntletSpawnWindow.h"
#include "UTPlayerState.h"
#include "SUTButton.h"
#include "UTGameMode.h"
#include "SUTBorder.h"

#include "SUTLoadoutUpgradeWindow.h"
#include "../SUTStyle.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

void SUTGauntletSpawnWindow::BuildWindow()
{
	SUTSpawnWindow::BuildWindow();
	BuildSpawnMessage(920);
	BuildLoadoutSlots(755);
}

EInputMode::Type SUTGauntletSpawnWindow::GetInputMode()
{
	return LoadoutUpgradeWindow.IsValid() ? EInputMode::EIM_UIOnly : EInputMode::EIM_GameAndUI;
}

bool SUTGauntletSpawnWindow::CanWindowClose()
{
	return !LoadoutUpgradeWindow.IsValid();
}


void SUTGauntletSpawnWindow::BuildSpawnMessage(float YPosition)
{
	float BarHeight = 100.0f;
	Overlay->AddSlot().VAlign(VAlign_Fill).HAlign(HAlign_Fill)
	[
		SNew(SCanvas)
		+SCanvas::Slot()
		.Position(FVector2D(0,YPosition + 2.0f))
		.Size(FVector2D(1920.0f, BarHeight))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(0.0f,0.0f,20.0f,0.0f))
			[
				SNew(STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
				.Text(this, &SUTGauntletSpawnWindow::GetCurrentText)
			]
		]

		+SCanvas::Slot()
		.Position(FVector2D(0,YPosition - 48.0f))
		.Size(FVector2D(1920.0f, 32))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(FMargin(0.0f,0.0f,20.0f,0.0f))
			[
				SNew(STextBlock)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
				.Text(this, &SUTGauntletSpawnWindow::GetCurrencyText)
			]
		]


	];
}

FText SUTGauntletSpawnWindow::GetCurrencyText() const
{
	int32 Currency = 0;
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (UTPlayerState)
		{
			Currency = int32(UTPlayerState->GetAvailableCurrency());
		}
	}

	return FText::Format(NSLOCTEXT("SUTGuantletSpawnWindow","PointsFormat","{0} Points"),FText::AsNumber(Currency));
}

void SUTGauntletSpawnWindow::BuildLoadoutSlots(float YPosition)
{
	float BarHeight = 160.0f;
	Overlay->AddSlot().VAlign(VAlign_Fill).HAlign(HAlign_Fill)
	[
		SNew(SCanvas)
		+ SCanvas::Slot()
		.Position(FVector2D(0, YPosition))
		.Size(FVector2D(1920.0f, BarHeight))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(FMargin(60.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SBox).WidthOverride(256)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).HeightOverride(32.0f)
						[
							SNew(SUTBorder)
							.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
								[

									SNew(STextBlock)
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
									.Text(this, &SUTGauntletSpawnWindow::GetLoadoutText, 0)
								]
							]
						]
					]
					+SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).HeightOverride(128)
						[
							SNew(SUTButton)
							.OnClicked(this, &SUTGauntletSpawnWindow::ItemClick, false)
							.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.SuperDark")
							[
								SNew(SBox).WidthOverride(128).HeightOverride(256)
								[
									SNew(SImage)
									.Image(this, &SUTGauntletSpawnWindow::GetLoadoutImage, false)
								]
							]
						]
					]
				]
			]


			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(FMargin(15.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SBox).WidthOverride(192)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).HeightOverride(32.0f) 
						[
							SNew(SUTBorder)
							.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
								[
									SNew(STextBlock)
									.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
									.Text(this, &SUTGauntletSpawnWindow::GetLoadoutText, 1) // NSLOCTEXT("SUTGauntletSpawnWindow","Upgrade Available","Upgrade Available"))
								]
							]
						]
					]
					+SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).HeightOverride(96)
						[
							SNew(SUTButton)
							.OnClicked(this, &SUTGauntletSpawnWindow::ItemClick, true)
							.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.SuperDark")
							[
								SNew(SBox).WidthOverride(192).HeightOverride(96)
								[
									SNew(SImage)
									.Image(this, &SUTGauntletSpawnWindow::GetLoadoutImage, true)
								]
							]
						]
					]
				]
			]

		]
	];
}

FText SUTGauntletSpawnWindow::GetLoadoutText(int32 RoundMask) const
{
	bool bUpgradeAvailable = false;
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		AUTGameState* UTGameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (UTGameState)
		{
			float Currency = UTPlayerState->GetAvailableCurrency();
			for (int32 i = 0; i < UTGameState->AvailableLoadout.Num(); i++)
			{
				AUTReplicatedLoadoutInfo* Loadout =UTGameState->AvailableLoadout[i];
				if (Loadout && Loadout->RoundMask == RoundMask && !Loadout->bDefaultInclude && !Loadout->bUnlocked && Loadout->CurrentCost <= Currency)
				{
					bUpgradeAvailable = true;
					break;
				}
			}
		}
	}

	return bUpgradeAvailable ? NSLOCTEXT("SUTGauntletSpawnWindow","Upgrade Available","Upgrade Available") : NSLOCTEXT("SUTGauntletSpawnWindow","Change","Click to Change");
}

EVisibility SUTGauntletSpawnWindow::UpgradesAvailable(int32 RoundMask) const
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		AUTGameState* UTGameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
		if (UTGameState)
		{
			float Currency = UTPlayerState->GetAvailableCurrency();
			for (int32 i = 0; i < UTGameState->AvailableLoadout.Num(); i++)
			{
				AUTReplicatedLoadoutInfo* Loadout =UTGameState->AvailableLoadout[i];
				if (Loadout && Loadout->RoundMask == RoundMask && !Loadout->bDefaultInclude && !Loadout->bUnlocked && Loadout->CurrentCost <= Currency)
				{
					return EVisibility::Visible;
				}
			}
		}
	}
	
	return EVisibility::Collapsed;
}


const FSlateBrush* SUTGauntletSpawnWindow::GetLoadoutImage(bool bSecondary) const
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (UTPlayerState)
		{
			AUTReplicatedLoadoutInfo* Loadout = bSecondary ? UTPlayerState->SecondarySpawnInventory : UTPlayerState->PrimarySpawnInventory;
			if (Loadout)
			{
				Loadout->LoadItemImage();
				return Loadout->GetItemImage();
			}
		}
	}

	return SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");
}

FText SUTGauntletSpawnWindow::GetCurrentText() const
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (UTPlayerState && UTPlayerState->CriticalObject != nullptr)
		{
			return NSLOCTEXT("SpawnWindow","CriticalSpawn","ALT-Fire for Crit Spawn");
		}
	}

	return FText::GetEmpty();
}

FReply SUTGauntletSpawnWindow::ItemClick(bool bSecondary)
{
	if (!LoadoutUpgradeWindow.IsValid())
	{
		SAssignNew(LoadoutUpgradeWindow, SUTLoadoutUpgradeWindow, PlayerOwner, bSecondary);
		if (LoadoutUpgradeWindow.IsValid())
		{
			LoadoutUpgradeWindow->SpawnWindow = SharedThis(this);
			PlayerOwner->OpenWindow(LoadoutUpgradeWindow);
		}
	}
	return FReply::Handled();
}



void SUTGauntletSpawnWindow::CloseUpgrade()
{
	if (LoadoutUpgradeWindow.IsValid())
	{
		PlayerOwner->CloseWindow(LoadoutUpgradeWindow);
		LoadoutUpgradeWindow.Reset();
	}

}


#endif