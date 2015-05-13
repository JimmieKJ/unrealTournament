// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTBuyMenu.h"
#include "SUWindowsStyle.h"
#include "Widgets/SUTButton.h"
#include "SUTUtils.h"
#include "UTPlayerState.h"

#if !UE_SERVER


void SUTBuyMenu::CreateDesktop()
{
	CollectItems();

	float CurrentCurrency = 0;
	if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
	{
		AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
		if (PC && PC->UTPlayerState)
		{
			CurrentCurrency = PC->UTPlayerState->GetAvailableCurrency();
		}
	}

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
								.Text(NSLOCTEXT("SUTBuyMenu","BuyTitle","Quick Buy"))
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
					.HAlign(HAlign_Center)
					.AutoWidth()
					.Padding(10.0,0.0,20.0,0.0)
					[
						SNew(SBox).WidthOverride(1400)
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
									.Text(FText::Format(NSLOCTEXT("SUTBuyMenu","Status","{0} - Available Items"), FText::AsCurrency(CurrentCurrency)))
									.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.TitleTextStyle")
								]

								+SVerticalBox::Slot()
								.HAlign(HAlign_Center)
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
											.Image(this, &SUTBuyMenu::GetItemImage)
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
									.Text(this, &SUTBuyMenu::GetItemDescriptionText)
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
											.Text(NSLOCTEXT("SUTLoadoutMenu","Cancel","Cancel Change"))
											.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
											.OnClicked(this, &SUTBuyMenu::OnCancelledClicked)
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
						]
					]
				]
			]

		]

	];

	RefreshAvailableItemsList();
}

void SUTBuyMenu::CollectItems()
{
	// Create the loadout information.
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->AvailableLoadout.Num() > 0)
	{
		for (int32 i=0; i < GameState->AvailableLoadout.Num(); i++)
		{
			if (GameState->AvailableLoadout[i]->ItemClass && !GameState->AvailableLoadout[i]->bDefaultInclude)
			{
				TSharedPtr<FLoadoutData> D = FLoadoutData::Make(GameState->AvailableLoadout[i]);
				AvailableItems.Add( D );
			}
		}
	}
}

const FSlateBrush* SUTBuyMenu::GetItemImage() const
{
	if (CurrentItem.IsValid())
	{
		return CurrentItem->Image.Get();
	}

	return SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");
}

FText SUTBuyMenu::GetItemDescriptionText() const
{
	if (CurrentItem.IsValid() && CurrentItem->DefaultObject.IsValid())
	{
		return CurrentItem->DefaultObject->MenuDescription;
	}

	return NSLOCTEXT("UTWeapon","DefaultDescription","This space let intentionally blank");
}

FReply SUTBuyMenu::OnCancelledClicked()
{
	PlayerOwner->CloseLoadout();
	return FReply::Handled();
}


void SUTBuyMenu::RefreshAvailableItemsList()
{
	AvailableItems.Sort(FCompareByCost());
	if (AvailableItemsPanel.IsValid())
	{
		AvailableItemsPanel->ClearChildren();
		for (int32 Idx = 0; Idx < AvailableItems.Num(); Idx++)	
		{
			int32 Row = Idx / 5;
			int32 Col = Idx % 5;

			// Add the cell...
			AvailableItemsPanel->AddSlot(Col, Row).Padding(5.0, 5.0, 5.0, 5.0)
				[
					SNew(SBox)
					.WidthOverride(256)
					.HeightOverride(200)
					[
						SNew(SUTButton)
						.OnClicked(this, &SUTBuyMenu::OnAvailableClick, Idx)
						.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
						.ToolTip(SUTUtils::CreateTooltip(AvailableItems[Idx]->DefaultObject->DisplayName))
						.UTOnMouseOver(this, &SUTBuyMenu::AvailableUpdateItemInfo)
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

void SUTBuyMenu::AvailableUpdateItemInfo(int32 Index)
{
	if (Index >= 0 && Index <= AvailableItems.Num())
	{
		CurrentItem = AvailableItems[Index];
	}
}

FReply SUTBuyMenu::OnAvailableClick(int32 Index)
{
	if (Index >= 0 && Index <= AvailableItems.Num())
	{
		if (PlayerOwner.IsValid() && PlayerOwner->PlayerController)
		{
			AUTPlayerController* PC = Cast<AUTPlayerController>(PlayerOwner->PlayerController);
			if (PC && PC->UTPlayerState)
			{
				PC->UTPlayerState->ServerBuyLoadout(AvailableItems[Index]->LoadoutInfo.Get());
				PlayerOwner->CloseLoadout();
			}
		}
	}


	return FReply::Handled();
}

FReply SUTBuyMenu::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnCancelledClicked();
	}

	return FReply::Unhandled();
}


#endif