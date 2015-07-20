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
	bRequiresRefresh = true;
	BuildMapList();
}

void SUWMapVoteDialog::BuildMapList()
{
	if (MapBox.IsValid() && GameState.IsValid() && GameState->VoteTimer > 0)
	{
		// Flag us as having been refreshed.
		bRequiresRefresh = false;

		// Store the current # of votes so we can check later to see if additional maps have been replicated or removed.
		LastVoteCount = GameState->MapVoteList.Num();
		
		// Figure out the # of columns needed.
		int32 NoColumns = FMath::Min<int32>(GameState->MapVoteList.Num(), 6);

		MapBox->ClearChildren();
		if ( GameState->VoteTimer > 0 && GameState->VoteTimer <= 10 )
		{
			MapBox->AddSlot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("SUWMapVoewDialog","Finalists","Choose from one of these finalist"))
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
					.Text(NSLOCTEXT("SUWMapVoewDialog","Lead","Maps with the most votes..."))
					.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.BodyTextStyle")
				]
			];
		}

		TSharedPtr<SGridPanel> TopPanel;
		TSharedPtr<SGridPanel> MapPanel;

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

		LeadingVoteButtons.Empty();
		VoteButtons.Empty();

		GameState->SortVotes();

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

		// The top row holds the top 6 maps in the lead.  Not if you have more than 6 maps with equal votes it will only show the first 6.
		// If no map has been voted on yet, it shows an empty space.

		for (int32 i=0; i < GameState->MapVoteList.Num() && i < NoColumns; i++)
		{
			if (GameState->MapVoteList[i])
			{
				TWeakObjectPtr<AUTReplicatedMapInfo> MapVoteInfo = GameState->MapVoteList[i];
				TSharedPtr<SImage> ImageWidget;

				// If this map has votes or if we are in the final seconds, then show draw the map.
				if ( MapVoteInfo.IsValid()  && (MapVoteInfo->VoteCount > 0 || GameState->VoteTimer <= 10) )
				{
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

							LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateRaw(this, &SUWMapVoteDialog::TextureLoadComplete),0);
						}
						else
						{
							MapBrush = MapVoteInfo->MapBrush;
						}
					}

					TopPanel->AddSlot(i, 0).Padding(5.0,5.0,5.0,5.0)
					[
						SNew(SBox).WidthOverride(256)
						.HeightOverride(158)							
						[
							SNew(SButton)
							.OnClicked(this, &SUWMapVoteDialog::OnMapClick, MapVoteInfo)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
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
																SNew(SBorder)
																.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Black"))
															]
														]
													]
												]
												+SOverlay::Slot()
												[
													SNew(STextBlock)
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
									SNew(STextBlock)
									.Text(FText::FromString(MapVoteInfo->Title))
									.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
									.ColorAndOpacity(FLinearColor::Black)
								]
							]
						]
					];
				}

				// Otherwise draw a blank space for this map.

				else
				{
					TopPanel->AddSlot(i, 0).Padding(5.0,5.0,5.0,5.0)
					[
						SNew(SBox).WidthOverride(256)
						.HeightOverride(158)							
						[
							SNew(SButton)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.HAlign(HAlign_Center)
								.FillHeight(1.0)
								[
									SNew(STextBlock)
									.Text(NSLOCTEXT("SUWMapVoteDialog","WaitingForVoteText","????"))
									.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
									.ColorAndOpacity(FLinearColor::Black)
								]
							]
						]
					];
				}

				LeadingVoteButtons.Add( FVoteButton(NULL, MapVoteInfo, ImageWidget) );
			}
		}

		// We only draw the full list if we have > 10 seconds left in map voting.
		if (GameState->VoteTimer > 10)
		{

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

								LoadPackageAsync(Package, FLoadPackageAsyncDelegate::CreateRaw(this, &SUWMapVoteDialog::TextureLoadComplete),0);
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
								SNew(SButton)
								.OnClicked(this, &SUWMapVoteDialog::OnMapClick, MapVoteInfo)
								.ButtonStyle(SUWindowsStyle::Get(), "UT.ComplexButton")
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
																	SNew(SBorder)
																	.BorderImage(SUWindowsStyle::Get().GetBrush("UT.Background.Black"))
																]
															]
														]
													]
													+SOverlay::Slot()
													[
														SNew(STextBlock)
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
										SNew(STextBlock)
										.Text(FText::FromString(MapVoteInfo->Title))
										.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
										.ColorAndOpacity(FLinearColor::Black)
									]
								]
							]
						];

						VoteButtons.Add( FVoteButton(NULL, MapVoteInfo, ImageWidget) );
					}
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



FReply SUWMapVoteDialog::OnMapClick(TWeakObjectPtr<AUTReplicatedMapInfo> MapInfo)
{

	AUTPlayerState* OwnerPlayerState = Cast<AUTPlayerState>(GetPlayerOwner()->PlayerController->PlayerState);
	if (OwnerPlayerState)
	{
		OwnerPlayerState->RegisterVote(MapInfo.Get());
	}

	return FReply::Handled();
}

void SUWMapVoteDialog::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (GameState.IsValid())
	{
		if (bRequiresRefresh)
		{
			BuildMapList();
		}
		else
		{
			bool bNeedsUpdate = false;
			if (GameState->MapVoteList.Num() == LastVoteCount)
			{
				for (int32 i=0; i < GameState->MapVoteList.Num(); i++)
				{
					if (GameState->MapVoteList[i]->bNeedsUpdate)
					{
						bNeedsUpdate = true;
					}
					GameState->MapVoteList[i]->bNeedsUpdate = false;
				}
			}
			else
			{
				bNeedsUpdate = true;
			}

			if (bNeedsUpdate)
			{
				BuildMapList();
			}
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