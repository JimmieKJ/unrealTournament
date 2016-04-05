// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTLoadoutUpgradeWindow.h"
#include "../SUWindowsStyle.h"
#include "../Widgets/SUTButton.h"
#include "../SUTUtils.h"
#include "UTPlayerState.h"
#include "SUTGauntletSpawnWindow.h"
#include "SUTBorder.h"

#if !UE_SERVER

struct FCompareLoadout 
{
	FORCEINLINE bool operator()( const TWeakObjectPtr< AUTReplicatedLoadoutInfo> A, const TWeakObjectPtr< AUTReplicatedLoadoutInfo> B ) const 
	{
		return ( A->CurrentCost < B->CurrentCost );
	}
};


void SUTLoadoutUpgradeWindow::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner, bool bShowSecondayItems)
{
	bSecondaryItems = bShowSecondayItems;
	SUTWindowBase::Construct(SUTWindowBase::FArguments().bShadow(false), InPlayerOwner);
}


void SUTLoadoutUpgradeWindow::BuildWindow()
{
	CollectItems();

	int32 CurrentCurrency = 0;
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
		if (PC && PC->UTPlayerState)
		{
			CurrentCurrency = int32(PC->UTPlayerState->GetAvailableCurrency());
		}
	}

	Content->AddSlot()
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SCanvas)
		+SCanvas::Slot()
		.Position(FVector2D(0.0f,0.0f))
		.Size(FVector2D(1400.0f,  bSecondaryItems ? 760.0f : 730.0f))
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().VAlign(VAlign_Bottom)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().AutoHeight().VAlign(VAlign_Bottom).Padding(FMargin((bSecondaryItems ? 331.0f : 60.0f),0.0f,0.0f,0.0f))
				[
					SNew(SUTBorder)
					.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot().HAlign(HAlign_Center).AutoHeight()
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("SUTLoadoutUpgradeWindow","AvailableWeapons","Available Weapons"))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
						]
						+SVerticalBox::Slot().HAlign(HAlign_Left).AutoHeight()
						[
							SAssignNew(ItemsPanel,SGridPanel)
						]

					]
				]
			]
		]
	];

	RefreshAvailableItemsList();
}

void SUTLoadoutUpgradeWindow::CollectItems()
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);

	if (GameState && GameState->AvailableLoadout.Num() > 0)
	{
		for (int32 i=0; i < GameState->AvailableLoadout.Num(); i++)
		{
			if (GameState->AvailableLoadout[i]->ItemClass && GameState->AvailableLoadout[i]->RoundMask == (bSecondaryItems ? 1 : 0))
			{
				if (GameState->AvailableLoadout[i]->bUnlocked || GameState->AvailableLoadout[i]->bDefaultInclude || GameState->AvailableLoadout[i]->CurrentCost < (UTPlayerState ? UTPlayerState->GetAvailableCurrency() : 0.0f) )
				{
					AvailableItems.Add( GameState->AvailableLoadout[i] );
				}
			}
		}
	}
}

void SUTLoadoutUpgradeWindow::RefreshAvailableItemsList()
{
	AvailableItems.Sort(FCompareLoadout());
	if (ItemsPanel.IsValid())
	{
		ItemsPanel->ClearChildren();
		for (int32 Idx = 0; Idx < AvailableItems.Num(); Idx++)	
		{
			int32 Row = Idx / 5;
			int32 Col = Idx % 5;

			AvailableItems[Idx]->LoadItemImage();

			TSharedPtr<SOverlay> ButtonOverlay;

			// Add the cell...
			ItemsPanel->AddSlot(Col, Row).Padding(5.0, 5.0, 5.0, 5.0)
			[
				SNew(SBox)
				.WidthOverride(256)
				.HeightOverride(160)
				[
					SAssignNew(ButtonOverlay, SOverlay)
					+SOverlay::Slot()
					[
						SNew(SUTButton)
						.OnClicked(this, &SUTLoadoutUpgradeWindow::OnAvailableClick, Idx)
						.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.SuperDark")
						//.UTOnMouseOver(this, &SUTBuyWindow::AvailableUpdateItemInfo)
						.WidgetTag(Idx)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox)
								.WidthOverride(256)
								.HeightOverride(128)
								[
									SNew(SImage)
									.Image(AvailableItems[Idx]->GetItemImage())
								]
							]
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::FromString(AvailableItems[Idx]->ItemTag.ToString()))
								.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
							]

						]
					]
				]
			];

			if (ButtonOverlay.IsValid() && !AvailableItems[Idx]->bUnlocked && !AvailableItems[Idx]->bDefaultInclude)
			{
				ButtonOverlay->AddSlot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.AutoHeight()
					[
						SNew(SUTBorder)
						.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
						[
							SNew(STextBlock)
							.Text(FText::Format(NSLOCTEXT("SUTLoadoutUpgradeWindow","UnlockFormat","{0} points to unlock"), FText::AsNumber(AvailableItems[Idx]->CurrentCost)))
							.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small.Bold")
						]
					]
				];
			}

		}
	}
}


FReply SUTLoadoutUpgradeWindow::OnAvailableClick(int32 Index)
{
	if (AvailableItems.IsValidIndex(Index))
	{
		AUTPlayerState* UTPlayerState = Cast<AUTPlayerState>(PlayerOwner->PlayerController->PlayerState);
		if (UTPlayerState)
		{
			if (AvailableItems[Index]->bUnlocked || AvailableItems[Index]->bDefaultInclude)
			{
				UTPlayerState->ServerSelectLoadout(AvailableItems[Index]->ItemTag, bSecondaryItems);
			}
			else
			{
				UTPlayerState->ServerUnlockItem(AvailableItems[Index]->ItemTag, bSecondaryItems);
			}
		}
	}

	if (SpawnWindow.IsValid()) SpawnWindow->CloseUpgrade();
	return FReply::Handled();
}

FReply SUTLoadoutUpgradeWindow::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (SpawnWindow.IsValid()) SpawnWindow->CloseUpgrade();
	return FReply::Handled();
}

#endif