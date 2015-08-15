// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SUWMapVoteDialog.h"
#include "SUWindowsStyle.h"
#include "SUTUtils.h"
#include "UTGameEngine.h"
#include "UTLobbyMatchInfo.h"
#include "UTEpicDefaultRulesets.h"
#include "UTLobbyGameState.h"
#include "UTReplicatedMapInfo.h"

#if !UE_SERVER

void SUWMapVoteDialog::Construct(const FArguments& InArgs)
{
	DefaultLevelScreenshot = new FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, FVector2D(256.0, 128.0), NAME_None);
	GameState = InArgs._GameState;
	SUWDialog::Construct(SUWDialog::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	if (DialogContent.IsValid())
	{
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().FillHeight(1.0).HAlign(HAlign_Center)
			[
				SAssignNew(MapBox,SScrollBox)
			]
		];
	}
	BuildMapList();
}

void SUWMapVoteDialog::BuildMapList()
{
	if (MapBox.IsValid() && GameState.IsValid() && GameState->VoteTimer > 0)
	{
		GameState->SortVotes();

		// Store the current # of votes so we can check later to see if additional maps have been replicated or removed.
		NumMapInfos = GameState->MapVoteList.Num();

		MapBox->ClearChildren();
		if ( GameState->VoteTimer > 0 && GameState->VoteTimer <= 10 )
		{
			MapBox->AddSlot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWMapVoteDialog","Finalists","Choose from the finalists:"))
					.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.BodyTextStyle")
				]
			];
		}
		else
		{
			MapBox->AddSlot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWMapVoteDialog","Lead","Maps with the most votes:"))
					.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.BodyTextStyle")
				]
			];
		}

		MapBox->AddSlot()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0,0.0,0.0,60.0)
			[
				SAssignNew(TopPanel, SGridPanel)
			]
			+SVerticalBox::Slot()
			.Padding(0.0,0.0,0.0,10.0)
			[
				SAssignNew(MapPanel, SGridPanel)
			]
		];

		BuildTopVotes();
		BuildAllVotes();
	}
}

void SUWMapVoteDialog::BuildTopVotes()
{
	// Figure out the # of columns needed.
	int32 NoColumns = FMath::Min<int32>(GameState->MapVoteList.Num(), 6);

	LeadingVoteButtons.Empty();
	TopPanel->ClearChildren();

	// The top row holds the top 6 maps in the lead.  Now if you have more than 6 maps with equal votes it will only show the first 6.
	// If no map has been voted on yet, it shows an empty space.

	for (int32 i=0; i < NoColumns; i++)
	{
		TSharedPtr<SImage> ImageWidget;
		TSharedPtr<STextBlock> VoteCountText;
		TSharedPtr<STextBlock> MapTitle;
		TSharedPtr<SBorder> BorderWidget;
		TSharedPtr<SUTComboButton> VoteButton;


		TopPanel->AddSlot(i, 0).Padding(5.0,5.0,5.0,5.0)
		[
			SNew(SBox).WidthOverride(256)
			.HeightOverride(158)							
			[
				SAssignNew(VoteButton, SUTComboButton)
				.OnClicked(this, &SUWMapVoteDialog::OnLeadingMapClick, i)
				.IsToggleButton(true)
				.bRightClickOpensMenu(true)
				.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
				.MenuButtonStyle(SUWindowsStyle::Get(),"UT.ContextMenu.Button")
				.MenuButtonTextStyle(SUWindowsStyle::Get(),"UT.ContextMenu.TextStyle")
				.HasDownArrow(false)
				.ButtonContent()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.WidthOverride(256)
						.HeightOverride(128)							
						[
							SNew(SOverlay)
							+SOverlay::Slot()
							[
								SAssignNew(ImageWidget,SImage)
								.Visibility(EVisibility::Hidden)
							]
							+SOverlay::Slot()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot().FillHeight(1.0)
								.Padding(5.0,5.0,0.0,0.0)
								[
									SNew(SOverlay)
									+SOverlay::Slot()
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.AutoHeight()
										[
											SNew(SHorizontalBox)
											+SHorizontalBox::Slot()
											.AutoWidth()
											[
												SNew(SBox)
												.WidthOverride(50)
												.HeightOverride(50)
												[
													SAssignNew(BorderWidget,SBorder)
													.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Black"))
													.Visibility(EVisibility::Hidden)
												]
											]
										]
									]
									+SOverlay::Slot()
									[
										SAssignNew(VoteCountText,STextBlock)
										.Text(FText::AsNumber(0))
										.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.BodyTextStyle")
										.Visibility(EVisibility::Hidden)
									]
								]
							]
						]
					]
					+SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.AutoHeight()
					[
						SAssignNew(MapTitle, STextBlock)
						.Text(NSLOCTEXT("SUWMapVoteDialog","WaitingForVoteText","????"))
						.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
						.ColorAndOpacity(FLinearColor::Black)
					]
				]
			]
		];
		LeadingVoteButtons.Add( FVoteButton(NULL, NULL, VoteButton, ImageWidget, MapTitle, VoteCountText, BorderWidget) );
	}
	UpdateTopVotes();
}

void SUWMapVoteDialog::UpdateTopVotes()
{
	GameState->SortVotes();
	for (int32 i=0; i < LeadingVoteButtons.Num(); i++)
	{
		bool bClear = true;

		TWeakObjectPtr<AUTReplicatedMapInfo> MapVoteInfo;
		if (i >=0 && i < GameState->MapVoteList.Num() && GameState->MapVoteList[i] && (GameState->MapVoteList[i]->VoteCount > 0 || GameState->VoteTimer <= 10))
		{
			MapVoteInfo = GameState->MapVoteList[i];
		}

		if ( MapVoteInfo.IsValid() )
		{
			bClear = false;
			if ( LeadingVoteButtons[i].MapVoteInfo.Get() != MapVoteInfo.Get() )
			{
				LeadingVoteButtons[i].MapVoteInfo = MapVoteInfo;
				FSlateDynamicImageBrush* MapBrush = DefaultLevelScreenshot;

				if (MapVoteInfo->MapScreenshotReference != TEXT(""))
				{
					if (MapVoteInfo->MapBrush == nullptr)
					{
						FString Package = MapVoteInfo->MapScreenshotReference;
						const int32 Pos = Package.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
						if ( Pos != INDEX_NONE )
						{
							Package = Package.Left(Pos);
						}

						LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateSP(this, &SUWMapVoteDialog::LeaderTextureLoadComplete), 0);
					}
					else
					{
						LeadingVoteButtons[i].MapImage->SetImage(MapVoteInfo->MapBrush);
						LeadingVoteButtons[i].MapImage->SetVisibility(EVisibility::Visible);
					}

				}

				LeadingVoteButtons[i].MapTitle->SetText(MapVoteInfo->Title);
				LeadingVoteButtons[i].MapTitle->SetVisibility(EVisibility::Visible);

				LeadingVoteButtons[i].VoteCountText->SetText(FText::AsNumber(MapVoteInfo->VoteCount));
				LeadingVoteButtons[i].VoteCountText->SetVisibility(EVisibility::Visible);
				LeadingVoteButtons[i].BorderWidget->SetVisibility(EVisibility::Visible);
			}
			else if (MapVoteInfo->bNeedsUpdate)
			{
				LeadingVoteButtons[i].VoteCountText->SetText(FText::AsNumber(MapVoteInfo->VoteCount));
			}
		}

		if (bClear && LeadingVoteButtons[i].MapVoteInfo.IsValid())
		{
			LeadingVoteButtons[i].MapVoteInfo.Reset();
			LeadingVoteButtons[i].MapTitle->SetText(NSLOCTEXT("SUWMapVoteDialog","WaitingForVoteText","????"));
			LeadingVoteButtons[i].VoteCountText->SetVisibility(EVisibility::Hidden);
			LeadingVoteButtons[i].BorderWidget->SetVisibility(EVisibility::Hidden);
		}
	}
}


void SUWMapVoteDialog::BuildAllVotes()
{
	// Figure out the # of columns needed.
	int32 NoColumns = FMath::Min<int32>(GameState->MapVoteList.Num(), 6);

	VoteButtons.Empty();
	MapPanel->ClearChildren();

	// We only draw the full list if we have > 10 seconds left in map voting.
	if (GameState->VoteTimer > 10)
	{
		// We need 2 lists.. one sorted by votes (use the main one in the GameState) and one that is sorted alphabetically.
		// We will build that one here.
		
		TArray<AUTReplicatedMapInfo*>  AlphaSortedList;
		for (int32 i=0; i < GameState->MapVoteList.Num(); i++)
		{
			AUTReplicatedMapInfo* VoteInfo = GameState->MapVoteList[i];
			if (VoteInfo)
			{
				bool bAdd = true;
				for (int32 j=0; j < AlphaSortedList.Num(); j++)
				{
					if (AlphaSortedList[j]->Title > VoteInfo->Title)
					{
						AlphaSortedList.Insert(VoteInfo, j);
						bAdd = false;
						break;
					}
				}

				if (bAdd)
				{
					AlphaSortedList.Add(VoteInfo);
				}
			}
		}

		// Now do the maps sorted

		for (int32 i=0; i < AlphaSortedList.Num(); i++)
		{
			if (GameState->MapVoteList[i])
			{
				int32 Row = i / NoColumns;
				int32 Col = i % NoColumns;

				TWeakObjectPtr<AUTReplicatedMapInfo> MapVoteInfo = AlphaSortedList[i];
			
				if (MapVoteInfo.IsValid())
				{
					TSharedPtr<SImage> ImageWidget;
					TSharedPtr<STextBlock> MapTitle;
					TSharedPtr<STextBlock> VoteCountText;
					TSharedPtr<SUTComboButton> VoteButton;
					TSharedPtr<SBorder> BorderWidget;

					FSlateDynamicImageBrush* MapBrush = DefaultLevelScreenshot;

					if (MapVoteInfo->MapScreenshotReference != TEXT(""))
					{
						if (MapVoteInfo->MapBrush == nullptr)
						{
							FString Package = MapVoteInfo->MapScreenshotReference;
							const int32 Pos = Package.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromStart);
							if ( Pos != INDEX_NONE )
							{
								Package = Package.Left(Pos);
							}

							LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateSP(this, &SUWMapVoteDialog::TextureLoadComplete), 0);
						}
						else
						{
							MapBrush = MapVoteInfo->MapBrush;
						}
					}

					MapPanel->AddSlot(Col, Row).Padding(5.0,5.0,5.0,5.0)
					[
						SNew(SBox)
						.WidthOverride(256)
						.HeightOverride(158)							
						[
							SAssignNew(VoteButton, SUTComboButton)
							.OnClicked(this, &SUWMapVoteDialog::OnMapClick, MapVoteInfo)
							.IsToggleButton(true)
							.bRightClickOpensMenu(true)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
							.MenuButtonStyle(SUWindowsStyle::Get(),"UT.ContextMenu.Button")
							.MenuButtonTextStyle(SUWindowsStyle::Get(),"UT.ContextMenu.TextStyle")
							.HasDownArrow(false)
							.ButtonContent()
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SBox)
									.WidthOverride(256)
									.HeightOverride(128)							
									[
										SNew(SOverlay)
										+SOverlay::Slot()
										[
											SAssignNew(ImageWidget,SImage)
											.Image(MapBrush)
										]
										+SOverlay::Slot()
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot().FillHeight(1.0)
											.Padding(5.0,5.0,0.0,0.0)
											[
												SNew(SOverlay)
												+SOverlay::Slot()
												[
													SNew(SVerticalBox)
													+SVerticalBox::Slot()
													.AutoHeight()
													[
														SNew(SHorizontalBox)
														+SHorizontalBox::Slot()
														.AutoWidth()
														[
															SNew(SBox)
															.WidthOverride(50)
															.HeightOverride(50)
															[
																SAssignNew(BorderWidget,SBorder)
																.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Black"))
															]
														]
													]
												]
												+SOverlay::Slot()
												[
													SAssignNew(VoteCountText,STextBlock)
													.Text(FText::AsNumber(MapVoteInfo->VoteCount))
													.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.BodyTextStyle")
												]
											]
										]
									]
								]
								+SVerticalBox::Slot()
								.HAlign(HAlign_Center)
								.AutoHeight()
								[
									SAssignNew(MapTitle,STextBlock)
									.Text(FText::FromString(MapVoteInfo->Title))
									.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
									.ColorAndOpacity(FLinearColor::Black)
								]
							]
						]
					];

					VoteButtons.Add( FVoteButton(NULL, MapVoteInfo, VoteButton, ImageWidget, MapTitle, VoteCountText, BorderWidget) );
				}
			}
		}
	}
}

void SUWMapVoteDialog::TextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
	if (Result == EAsyncLoadingResult::Succeeded)
	{
		for (int32 i=0 ;i < VoteButtons.Num(); i++)
		{
			if (VoteButtons[i].MapVoteInfo.IsValid())
			{
				FString Screenshot = VoteButtons[i].MapVoteInfo->MapScreenshotReference;
				FString PackageName = InPackageName.ToString();
				if (Screenshot != TEXT("") && Screenshot.Contains(PackageName))
				{
					UTexture2D* Tex = FindObject<UTexture2D>(nullptr, *Screenshot);
					if (Tex)
					{
						VoteButtons[i].MapTexture = Tex;
						VoteButtons[i].MapVoteInfo->MapBrush = new FSlateDynamicImageBrush(Tex, FVector2D(256.0, 128.0), NAME_None);
						VoteButtons[i].MapImage->SetImage(VoteButtons[i].MapVoteInfo->MapBrush);
					}
				}
			}
		}
	}
}

void SUWMapVoteDialog::LeaderTextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
	if (Result == EAsyncLoadingResult::Succeeded)
	{
		for (int32 i=0 ;i < LeadingVoteButtons.Num(); i++)
		{
			if (LeadingVoteButtons[i].MapVoteInfo.IsValid())
			{
				FString Screenshot = LeadingVoteButtons[i].MapVoteInfo->MapScreenshotReference;
				FString PackageName = InPackageName.ToString();
				if (Screenshot != TEXT("") && Screenshot.Contains(PackageName))
				{
					UTexture2D* Tex = FindObject<UTexture2D>(nullptr, *Screenshot);
					if (Tex)
					{
						LeadingVoteButtons[i].MapTexture = Tex;
						LeadingVoteButtons[i].MapVoteInfo->MapBrush = new FSlateDynamicImageBrush(Tex, FVector2D(256.0, 128.0), NAME_None);
						LeadingVoteButtons[i].MapImage->SetImage(LeadingVoteButtons[i].MapVoteInfo->MapBrush);
						LeadingVoteButtons[i].MapImage->SetVisibility(EVisibility::Visible);
					}
				}
			}
		}
	}
}



FReply SUWMapVoteDialog::OnLeadingMapClick(int32 ButtonIndex)
{
	AUTPlayerState* OwnerPlayerState = Cast<AUTPlayerState>(GetPlayerOwner()->PlayerController->PlayerState);
	if (OwnerPlayerState && ButtonIndex >=0 && ButtonIndex <= LeadingVoteButtons.Num() && LeadingVoteButtons[ButtonIndex].MapVoteInfo.IsValid())
	{
		OwnerPlayerState->RegisterVote(LeadingVoteButtons[ButtonIndex].MapVoteInfo.Get());

		// Now find the button..

		for (int32 i=0; i < LeadingVoteButtons.Num(); i++)
		{
			if ( i == ButtonIndex )
			{
				if (LeadingVoteButtons[i].VoteButton.IsValid())
				{
					LeadingVoteButtons[i].VoteButton->BePressed();
				}
			}
			else
			{
				if (LeadingVoteButtons[i].VoteButton.IsValid())
				{
					LeadingVoteButtons[i].VoteButton->UnPressed();
				}
			}
		}

		for (int32 i=0; i < VoteButtons.Num(); i++)
		{
			if (VoteButtons[i].VoteButton.IsValid())
			{
				VoteButtons[i].VoteButton->UnPressed();
			}
		}
	}

	return FReply::Handled();
}

FReply SUWMapVoteDialog::OnMapClick(TWeakObjectPtr<AUTReplicatedMapInfo> MapInfo)
{
	AUTPlayerState* OwnerPlayerState = Cast<AUTPlayerState>(GetPlayerOwner()->PlayerController->PlayerState);
	if (OwnerPlayerState)
	{
		OwnerPlayerState->RegisterVote(MapInfo.Get());

		// Now find the button..

		for (int32 i=0; i < VoteButtons.Num(); i++)
		{
			if (VoteButtons[i].MapVoteInfo.Get() == MapInfo.Get())
			{
				if (VoteButtons[i].VoteButton.IsValid())
				{
					VoteButtons[i].VoteButton->BePressed();
				}
			}
			else
			{
				if (VoteButtons[i].VoteButton.IsValid())
				{
					VoteButtons[i].VoteButton->UnPressed();
				}
			}
		}

		for (int32 i=0; i < LeadingVoteButtons.Num(); i++)
		{
			if (LeadingVoteButtons[i].VoteButton.IsValid())
			{
				LeadingVoteButtons[i].VoteButton->UnPressed();
			}
		}



	}

	return FReply::Handled();
}


void SUWMapVoteDialog::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (GameState.IsValid())
	{
		bool bNeedsFullUpdate = false;
		if (GameState->MapVoteList.Num() == NumMapInfos)
		{
			UpdateTopVotes();

			for (int32 i=0; i < VoteButtons.Num(); i++)
			{
				if (VoteButtons[i].MapVoteInfo.IsValid() && VoteButtons[i].MapVoteInfo->bNeedsUpdate)
				{
					VoteButtons[i].VoteCountText->SetText(FText::AsNumber(VoteButtons[i].MapVoteInfo->VoteCount));
					GameState->MapVoteList[i]->bNeedsUpdate = false;
				}
			}
		}
		else
		{
			bNeedsFullUpdate = true;
		}

		if (bNeedsFullUpdate)
		{
			BuildMapList();
		}
	}
}

TSharedRef<class SWidget> SUWMapVoteDialog::BuildCustomButtonBar()
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot().Padding(20.0,0.0,0.0,0.0)
		[
			SNew(STextBlock)
			.Text(this, &SUWMapVoteDialog::GetClockTime)
			.TextStyle(SUWindowsStyle::Get(), "UT.TopMenu.Button.SmallTextStyle")
			.ColorAndOpacity(FLinearColor::Yellow)
		];
}


FText SUWMapVoteDialog::GetClockTime() const
{
	if (GameState.IsValid())
	{
		if (GameState->VoteTimer > 10)
		{
			return FText::Format(NSLOCTEXT("SUWMapVoteDialog","ClockFormat","Voting ends in {0} seconds..."),  FText::AsNumber(GameState->VoteTimer));
		}
		else
		{
			return FText::Format(NSLOCTEXT("SUWMapVoteDialog","ClockFormat2","Hurry, final voting ends in {0} seconds..."),  FText::AsNumber(GameState->VoteTimer));
		}
	}
	return FText::GetEmpty();
}

void SUWMapVoteDialog::OnDialogClosed()
{
	for (int32 i=0; i < VoteButtons.Num(); i++)
	{
		VoteButtons[i].MapImage = nullptr;
	}

	VoteButtons.Empty();
	SUWDialog::OnDialogClosed();
}

#endif