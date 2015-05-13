// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTLoadoutMenu.h"
#include "SUWindowsStyle.h"
#include "Widgets/SUTButton.h"
#include "SUTUtils.h"
#include "UTPlayerState.h"

#if !UE_SERVER

void SUTLoadoutMenu::CreateDesktop()
{
	CollectItems();

	ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[

		SNew(SOverlay)
		
		// Shadowed background
		+SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SImage)
			.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.Shadow"))
		]

		// Actual Menu
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			// Top Bar

			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1.0)
				[
					SNew(SBox)
					.HeightOverride(16)
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UT.Loadout.UpperBar"))
					]
				]
			]

			// Title Tab

			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(32.0f,0.0f,0.0f,0.0f)
				.AutoWidth()
				[			
					SNew(SBox)
					.WidthOverride(350)
					.HeightOverride(56)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SImage)
							.Image(SUWindowsStyle::Get().GetBrush("UT.Loadout.Tab"))
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(5.0,2.0,5.0,0.0)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("SUTLoadoutMenu","LoadoutTitle","Loadout"))
								.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
							]
						]
					]
				]
			]

			// The Menu itself...

			+SVerticalBox::Slot()
			.Padding(10.0f,30.0f, 10.0f,10.0f)
			.HAlign(HAlign_Center)
			.FillHeight(1.0)
			[
				// The List portion
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.AutoWidth()
					.Padding(10.0,0.0,20.0,0.0)
					[
						SNew(SBox).WidthOverride(800)
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Loadout.List.Normal"))
							]
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.HAlign(HAlign_Right)
								.AutoHeight()
								.Padding(10.0,0.0,10.0,0.0)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("Available Items")))
									.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.TitleTextStyle")
								]

								+SVerticalBox::Slot()
								.HAlign(HAlign_Fill)
								[
									SNew(SScrollBox)
									+SScrollBox::Slot()
									[
										SAssignNew(AvailableItemsPanel, SGridPanel)
									]
								]
							]
						]
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(20.0, 0.0, 10.0, 0.0)
					[
						SNew(SBox).WidthOverride(800)
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Loadout.List.Normal"))
							]
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.Padding(10.0,0.0,10.0,0.0)
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(this, &SUTLoadoutMenu::GetLoadoutTitle)
									.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.TitleTextStyle")
								]

								+SVerticalBox::Slot()
								[
									SNew(SScrollBox)
									+ SScrollBox::Slot()
									[
										SAssignNew(SelectedItemsPanel, SGridPanel)
									]
								]
							]
						]
					]
				]

				// The Info portion
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(20.0,20.0,20.0,0.0)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1.0)
					[
						SNew(SBox)
						.WidthOverride(1400)
						.HeightOverride(250)
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Loadout.List.Normal"))
							]
							+SOverlay::Slot()
							[
								SNew(SHorizontalBox)
							
								+SHorizontalBox::Slot()
								.AutoWidth()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SBox)
										.WidthOverride(256)
										.HeightOverride(128)
										[
											SNew(SImage)
											.Image(this, &SUTLoadoutMenu::GetItemImage)
										]
									]
								]
								+SHorizontalBox::Slot()
								.FillWidth(1.0)
								[
									SNew(SRichTextBlock)
									.TextStyle(SUWindowsStyle::Get(),"UT.Hub.RulesText")
									.Justification(ETextJustify::Left)
									.DecoratorStyleSet( &SUWindowsStyle::Get() )
									.AutoWrapText( true )
									.Text(this, &SUTLoadoutMenu::GetItemDescriptionText)

									
								]
							]
						]
					]	
				]
			]

		
			// The ButtonBar
			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Bottom)
			.Padding(5.0f, 5.0f, 5.0f, 5.0f)
			[
				SNew(SBox)
				.HeightOverride(48)
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.FillWidth(1.0)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Fill)
								[
									SNew(SImage)
									.Image(SUWindowsStyle::Get().GetBrush("UT.TopMenu.DarkFill"))
								]
							]
							+ SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Right)
								[
									SNew(SUniformGridPanel)
									.SlotPadding(FMargin(0.0f,0.0f, 10.0f, 10.0f))

									+SUniformGridPanel::Slot(0,0)
									.HAlign(HAlign_Fill)
									[
										SNew(SBox)
										.HeightOverride(140)
										[
											SNew(SButton)
											.HAlign(HAlign_Center)
											.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
											.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
											.Text(NSLOCTEXT("SUTLoadoutMenu","Accept","Accept Loadout"))
											.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
											.OnClicked(this, &SUTLoadoutMenu::OnAcceptClicked)
											.IsEnabled(this, &SUTLoadoutMenu::OnAcceptEnabled)
										]
									]

									+SUniformGridPanel::Slot(1,0)
									.HAlign(HAlign_Fill)
									[
										SNew(SBox)
										.HeightOverride(140)
										[
											SNew(SButton)
											.HAlign(HAlign_Center)
											.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
											.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
											.Text(NSLOCTEXT("SUTLoadoutMenu","Cancel","Cancel Change"))
											.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
											.OnClicked(this, &SUTLoadoutMenu::OnCancelledClicked)
										]
									]

								]

							]
						]

					]
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Left)
						.Padding(10.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SCanvas)
							//BuildCustomButtonBar()
						]
					]
				]
			]

		]

	];

	RefreshAvailableItemsList();
	RefreshSelectedItemsList();
}

void SUTLoadoutMenu::CollectItems()
{
	// Create the loadout information.
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->AvailableLoadout.Num() > 0)
	{
		for (int32 i=0; i < GameState->AvailableLoadout.Num(); i++)
		{
			if (GameState->AvailableLoadout[i]->ItemClass && !GameState->AvailableLoadout[i]->bPurchaseOnly)
			{
				TSharedPtr<FLoadoutData> D = FLoadoutData::Make(GameState->AvailableLoadout[i]);
				if (GameState->AvailableLoadout[i]->bDefaultInclude)
				{
					SelectedItems.Add( D );
				}
				else
				{
					AvailableItems.Add( D );
				}
			}
		}
	}
	TallyLoadout();
}

void SUTLoadoutMenu::TallyLoadout()
{
	TotalCostOfCurrentLoadout = 0.0f;
	for (int32 i=0; i < SelectedItems.Num(); i++)
	{
		if (SelectedItems[i].IsValid() && SelectedItems[i]->LoadoutInfo.IsValid())
		{
			TotalCostOfCurrentLoadout += SelectedItems[i]->LoadoutInfo->CurrentCost;
		}
	}

}

FText SUTLoadoutMenu::GetLoadoutTitle() const
{
	float CurrentCurrency = 0;
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
		if (PC && PC->UTPlayerState)
		{
			CurrentCurrency = PC->UTPlayerState->GetAvailableCurrency();
		}
	}
	
	FText CurrentCash = FText::AsNumber(CurrentCurrency);
	return FText::Format(NSLOCTEXT("SUTLoadoutMenuu","LoadoutTitleFormat","Loadout... ${0}/${1}"), FText::AsNumber(TotalCostOfCurrentLoadout), CurrentCash);
}

const FSlateBrush* SUTLoadoutMenu::GetItemImage() const
{
	if (CurrentItem.IsValid())
	{
		return CurrentItem->Image.Get();
	}

	return SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");
}

FText SUTLoadoutMenu::GetItemDescriptionText() const
{
	if (CurrentItem.IsValid() && CurrentItem->DefaultObject.IsValid())
	{
		return CurrentItem->DefaultObject->MenuDescription;
	}

	return NSLOCTEXT("UTWeapon","DefaultDescription","This space let intentionally blank");
}

FReply SUTLoadoutMenu::OnAcceptClicked()
{
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
		if (PC && PC->UTPlayerState)
		{
			TArray<AUTReplicatedLoadoutInfo*> NewLoadout;
			for (int32 i=0; i < SelectedItems.Num(); i++)
			{
				if (SelectedItems[i].IsValid() && SelectedItems[i]->LoadoutInfo.IsValid() )
				{
					NewLoadout.Add(SelectedItems[i]->LoadoutInfo.Get());
				}
			}

			PC->UTPlayerState->ServerUpdateLoadout(NewLoadout);
		}
	}

	PlayerOwner->CloseLoadout();
	return FReply::Handled();
}

FReply SUTLoadoutMenu::OnCancelledClicked()
{
	PlayerOwner->CloseLoadout();
	return FReply::Handled();
}

bool SUTLoadoutMenu::OnAcceptEnabled() const
{
	float CurrentCurrency = 0;
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
		if (PC && PC->UTPlayerState)
		{
			CurrentCurrency = PC->UTPlayerState->GetAvailableCurrency();
		}
	}

	return TotalCostOfCurrentLoadout <= CurrentCurrency;

}

void SUTLoadoutMenu::RefreshAvailableItemsList()
{
	AvailableItems.Sort(FCompareByCost());
	if (AvailableItemsPanel.IsValid())
	{
		AvailableItemsPanel->ClearChildren();
		for (int32 Idx = 0; Idx < AvailableItems.Num(); Idx++)	
		{
			int32 Row = Idx / 3;
			int32 Col = Idx % 3;

			// Add the cell...
			AvailableItemsPanel->AddSlot(Col, Row).Padding(5.0, 5.0, 5.0, 5.0)
				[
					SNew(SBox)
					.WidthOverride(256)
					.HeightOverride(200)
					[
						SNew(SUTButton)
						.OnClicked(this, &SUTLoadoutMenu::OnAvailableClick, Idx)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
						.ToolTip(SUTUtils::CreateTooltip(AvailableItems[Idx]->DefaultObject->DisplayName))
						.UTOnMouseOver(this, &SUTLoadoutMenu::AvailableUpdateItemInfo)
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
									.Image(AvailableItems[Idx]->Image.Get())
								]
							]
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(AvailableItems[Idx]->DefaultObject->DisplayName)
								.TextStyle(SUWindowsStyle::Get(), "UT.Hub.MapsText")
								.ColorAndOpacity(FLinearColor::Black)
							]

							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Bottom)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(FText::AsCurrency(AvailableItems[Idx]->LoadoutInfo->CurrentCost))
								.TextStyle(SUWindowsStyle::Get(), "UT.Hub.MapsText")
								.ColorAndOpacity(FLinearColor::Black)
							]
						]
					]
				];
		}
	}
}
void SUTLoadoutMenu::RefreshSelectedItemsList()
{
	SelectedItems.Sort(FCompareByCost());
	if (SelectedItemsPanel.IsValid())
	{
		SelectedItemsPanel->ClearChildren();
		for (int32 Idx = 0; Idx < SelectedItems.Num(); Idx++)
		{
			int32 Row = Idx / 3;
			int32 Col = Idx % 3;

			// Add the cell...
			SelectedItemsPanel->AddSlot(Col, Row).Padding(5.0, 5.0, 5.0, 5.0)
				[
					SNew(SBox)
					.WidthOverride(256)
					.HeightOverride(200)
					[
						SNew(SButton)
						.OnClicked(this, &SUTLoadoutMenu::OnSelectedClick, Idx)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
						.ToolTip(SUTUtils::CreateTooltip(SelectedItems[Idx]->DefaultObject->DisplayName))
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
									.Image(SelectedItems[Idx]->Image.Get())
								]
							]
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Text(SelectedItems[Idx]->DefaultObject->DisplayName)
								.TextStyle(SUWindowsStyle::Get(), "UT.Hub.MapsText")
								.ColorAndOpacity(FLinearColor::Black)
							]
						]
					]
				];
		}
	}
}

void SUTLoadoutMenu::AvailableUpdateItemInfo(int32 Index)
{
	if (Index >= 0 && Index <= AvailableItems.Num())
	{
		CurrentItem = AvailableItems[Index];
	}
}

FReply SUTLoadoutMenu::OnAvailableClick(int32 Index)
{
	if (Index >= 0 && Index <= AvailableItems.Num())
	{
		SelectedItems.Add(AvailableItems[Index]);
		AvailableItems.RemoveAt(Index);
		RefreshAvailableItemsList();
		RefreshSelectedItemsList();
		TallyLoadout();

	}


	return FReply::Handled();
}

FReply SUTLoadoutMenu::OnSelectedClick(int32 Index)
{
	if (Index >= 0 && Index <= SelectedItems.Num())
	{
		if (!SelectedItems[Index]->LoadoutInfo->bDefaultInclude)
		{
			AvailableItems.Add(SelectedItems[Index]);
			SelectedItems.RemoveAt(Index);
			RefreshAvailableItemsList();
			RefreshSelectedItemsList();
			TallyLoadout();
		}
	}


	return FReply::Handled();
}

FReply SUTLoadoutMenu::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCancelledClicked();
	}

	return FReply::Unhandled();
}


#endif