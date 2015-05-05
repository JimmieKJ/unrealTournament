// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUTLoadoutMenu.h"
#include "SUWindowsStyle.h"
#include "UTPlayerState.h"

#if !UE_SERVER

void SUTLoadoutMenu::CreateDesktop()
{
	CollectWeapons();

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
								.Text(NSLOCTEXT("SUTLoadoutMenu","LoadoutTitle","Weapon Loadout"))
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
					[
						SNew(SBox).WidthOverride(700)
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
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("Available Weapons")))
									.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.TitleTextStyle")
								]

								+SVerticalBox::Slot()
								.HAlign(HAlign_Fill)
								[
									SAssignNew(AvailableWeaponList, SListView< TSharedPtr<FLoadoutData> >)
									.ItemHeight(96.0)
									.ListItemsSource( &AvailableWeapons)
									.OnGenerateRow( this, &SUTLoadoutMenu::GenerateAvailableLoadoutInfoListWidget)
									.OnSelectionChanged( this, &SUTLoadoutMenu::AvailableWeaponChanged)
									.SelectionMode(ESelectionMode::Single)
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(10.0,0.0,10.0,0.0)
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(240)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SButton)
								.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
								.OnClicked(this, &SUTLoadoutMenu::OnAddClicked)
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot().HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::FromString(TEXT("Add :>>")))
										.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.TitleTextStyle")
									]
								]
							]
							+SVerticalBox::Slot()
							.Padding(0.0,15.0,0.0,0.0)
							.AutoHeight()
							[
								SNew(SButton)
								.ButtonStyle(SUWindowsStyle::Get(), "UT.BottomMenu.Button")
								.OnClicked(this, &SUTLoadoutMenu::OnRemovedClicked)
								[
									SNew(SHorizontalBox)
									+SHorizontalBox::Slot().HAlign(HAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::FromString(TEXT("<<: Remove")))
										.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.TitleTextStyle")
									]
								]
							]
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox).WidthOverride(700)
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
								.AutoHeight()
								[
									SNew(STextBlock)
									.Text(this, &SUTLoadoutMenu::GetLoadoutTitle)
									.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.TitleTextStyle")
								]

								+SVerticalBox::Slot()
								[
									SAssignNew(SelectedWeaponList, SListView< TSharedPtr<FLoadoutData> >)
									.ItemHeight(96.0)
									.ListItemsSource( &SelectedWeapons)
									.OnGenerateRow( this, &SUTLoadoutMenu::GenerateSelectedLoadoutInfoListWidget )
									.OnSelectionChanged( this, &SUTLoadoutMenu::SelectedWeaponChanged)
									.SelectionMode(ESelectionMode::Single)
	
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
											.Image(this, &SUTLoadoutMenu::GetDescriptionImage)
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
									.Text(this, &SUTLoadoutMenu::GetDescriptionText)

									
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

	if (AvailableWeapons.Num() >0)
	{
		AvailableWeaponList->SetSelection(AvailableWeapons[0]);
	}
}

void SUTLoadoutMenu::CollectWeapons()
{
	// Create the loadout information.
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState && GameState->LoadoutWeapons.Num() > 0)
	{
		for (int32 i=0; i < GameState->LoadoutWeapons.Num(); i++)
		{
			if (GameState->LoadoutWeapons[i]->WeaponClass)
			{
				TSharedPtr<FLoadoutData> D = FLoadoutData::Make(GameState->LoadoutWeapons[i]);
				AvailableWeapons.Add( D );
			}
		}
	}
	TallyLoadout();
}

TSharedRef<ITableRow> SUTLoadoutMenu::GenerateAvailableLoadoutInfoListWidget( TSharedPtr<FLoadoutData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FSimpleListData>>, OwnerTable)
		.Style(SUWindowsStyle::Get(),"UT.Loadout.List.Row")
		[

			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(600).HeightOverride(140)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Right)
						[
							SNew(STextBlock)
							.Text(InItem->DefaultWeaponObject->DisplayName)
							.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.BodyTextStyle")
						]
						+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBox).WidthOverride(192).HeightOverride(96)
								[
									SNew(SImage)
									.Image(InItem->WeaponImage.Get())
								]
							]
						]
					]
				]
			]
		];
}

TSharedRef<ITableRow> SUTLoadoutMenu::GenerateSelectedLoadoutInfoListWidget( TSharedPtr<FLoadoutData> InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow<TSharedPtr<FSimpleListData>>, OwnerTable)
		.Style(SUWindowsStyle::Get(),"UT.Loadout.List.Row")
		[

			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SBox).WidthOverride(600).HeightOverride(140)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot().AutoHeight()
							[
								SNew(SBox).WidthOverride(192).HeightOverride(96)
								[
									SNew(SImage)
									.Image(InItem->WeaponImage.Get())
								]
							]
						]
						+SHorizontalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.Text(InItem->DefaultWeaponObject->DisplayName)
							.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.BodyTextStyle")
						]
					]
				]
			]
		];
}

FReply SUTLoadoutMenu::OnAddClicked()
{
	TArray<TSharedPtr<FLoadoutData>> SelectedItems = AvailableWeaponList->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		TSharedPtr<FLoadoutData> Selected = SelectedItems[0];
		if (Selected.IsValid())
		{
			AvailableWeapons.Remove(Selected);
			SelectedWeapons.Add(Selected);
		}
	}

	AvailableWeaponList->RequestListRefresh();
	SelectedWeaponList->RequestListRefresh();
	TallyLoadout();

	return FReply::Handled();
}

FReply SUTLoadoutMenu::OnRemovedClicked()
{
	TArray<TSharedPtr<FLoadoutData>> SelectedItems = SelectedWeaponList->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		TSharedPtr<FLoadoutData> Selected = SelectedItems[0];
		if (Selected.IsValid())
		{
			SelectedWeapons.Remove(Selected);
			AvailableWeapons.Add(Selected);
		}
	}

	AvailableWeaponList->RequestListRefresh();
	SelectedWeaponList->RequestListRefresh();
	TallyLoadout();
	return FReply::Handled();
}

void SUTLoadoutMenu::TallyLoadout()
{
	TotalCostOfCurrentLoadout = 0.0f;
	for (int32 i=0; i < SelectedWeapons.Num(); i++)
	{
		if (SelectedWeapons[i].IsValid() && SelectedWeapons[i]->LoadoutInfo.IsValid())
		{
			TotalCostOfCurrentLoadout += SelectedWeapons[i]->LoadoutInfo->CurrentCost;
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
	
	FText CurrentCash = FText::AsNumber(0);
	return FText::Format(NSLOCTEXT("SUTLoadoutMenuu","LoadoutTitleFormat","Loadout... ${0}/${1}"), FText::AsNumber(TotalCostOfCurrentLoadout), CurrentCash);
}


void SUTLoadoutMenu::AvailableWeaponChanged(TSharedPtr<FLoadoutData> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		CurrentWeapon = NewSelection;
	}
}

void SUTLoadoutMenu::SelectedWeaponChanged(TSharedPtr<FLoadoutData> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		CurrentWeapon = NewSelection;
	}
}

const FSlateBrush* SUTLoadoutMenu::GetDescriptionImage() const
{
	if (CurrentWeapon.IsValid())
	{
		return CurrentWeapon->WeaponImage.Get();
	}

	return SUWindowsStyle::Get().GetBrush("UWindows.Lobby.MatchBadge");
}

FText SUTLoadoutMenu::GetDescriptionText() const
{
	if (CurrentWeapon.IsValid() && CurrentWeapon->DefaultWeaponObject.IsValid())
	{
		return CurrentWeapon->DefaultWeaponObject->MenuDescription;
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
			for (int32 i=0; i < SelectedWeapons.Num(); i++)
			{
				if (SelectedWeapons[i].IsValid() && SelectedWeapons[i]->LoadoutInfo.IsValid() )
				{
					NewLoadout.Add(SelectedWeapons[i]->LoadoutInfo.Get());
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

#endif