
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUTChallengePanel.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "../SUWScaleBox.h"
#include "../Widgets/SUTButton.h"
#include "UTChallengeManager.h"
#include "UTAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Analytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "../SUWindowsMainMenu.h"
#include "UTLevelSummary.h"
#include "UTGameEngine.h"

#if !UE_SERVER

SUTChallengePanel::~SUTChallengePanel()
{
	if (LevelScreenshot != NULL)
	{
		delete LevelScreenshot;
		LevelScreenshot = NULL;
	}
}
void SUTChallengePanel::ConstructPanel(FVector2D ViewportSize)
{
	LevelShot = nullptr;

	SelectedChallenge = NAME_None;
	ChallengeManager = Cast<UUTGameEngine>(GEngine)->GetChallengeManager();

	LastChallengeRevisionNumber = ChallengeManager.IsValid() ? ChallengeManager->RevisionNumber : 0;
	LevelScreenshot = new FSlateDynamicImageBrush(GEngine->DefaultTexture, FVector2D(256.0f, 128.0f), NAME_None);

	this->ChildSlot
	.VAlign(VAlign_Fill)
	.HAlign(HAlign_Fill)
	[
		SNew(SOverlay)
		+SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Fill)
				[
					SNew(SUWScaleBox)
					.bMaintainAspectRatio(false)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush("UT.HomePanel.Background"))
					]
				]
			]
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(64.0, 15.0, 64.0, 15.0)
			.FillHeight(1.0)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox).WidthOverride(1792).HeightOverride(860)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f,0.0f,8.0f,0.0f)
						[
							SNew(SBox).WidthOverride(900)
							[
								SNew(SVerticalBox)

								+SVerticalBox::Slot()
								.Padding(0.0,0.0,0.0,16.0)
								.AutoHeight()
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
									.FillHeight(1.0)
									.VAlign(VAlign_Center)
									[
										SNew(SButton)
										.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
										.OnClicked(this, &SUTChallengePanel::CustomClicked)
										[
											SNew(SBox).HeightOverride(96)
											[
												SNew(SBorder)
												.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
												[
													SNew(SHorizontalBox)
													+SHorizontalBox::Slot()
													.HAlign(HAlign_Center)
													.VAlign(VAlign_Center)
													[
														SNew(STextBlock)
														.Text(NSLOCTEXT("SUTChallengePanel","CustomChallenge","Create your own custom match."))
														.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium")
													]
												]									
											]
										]
									]
								]

								+SVerticalBox::Slot()
								.Padding(0.0,0.0,0.0,16.0)
								.AutoHeight()
								[
									SNew(SBorder)
									.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.HAlign(HAlign_Center)
										[
											SNew(STextBlock)
											.Text(NSLOCTEXT("SUTChallengePanel","AvailableChallenges","AVAILABLE CHALLENGES"))
											.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium.Bold")
										]
									]
								]

								+SVerticalBox::Slot()
								.FillHeight(1.0)
								[
									SNew(SBorder)
									.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
									[
										SNew(SScrollBox)
										.Style(SUWindowsStyle::Get(),"UT.ScrollBox.Borderless")
										+SScrollBox::Slot()
										[
											SAssignNew(ChallengeBox,SVerticalBox)
										]
									]
								]


							]
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(8.0f,0.0f,0.0f,0.0f)
						[
							SNew(SBox).WidthOverride(876)
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.Padding(0.0,0.0,0.0,16.0)
								.AutoHeight()
								[
									SNew(SBox).HeightOverride(64)
									[
										SNew(SBorder)
										.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
										[
											SNew(SVerticalBox)
											+SVerticalBox::Slot()
											.HAlign(HAlign_Right)
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												[
													SNew(STextBlock)
													.Text(this, &SUTChallengePanel::GetYourScoreText)
													.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
												]
											]
										]
									]
								]
								+SVerticalBox::Slot()
								.Padding(0.0,0.0,0.0,16.0)
								.AutoHeight()
								[
									SNew(SBox).HeightOverride(438)
									[
										SNew(SImage)
										.Image(LevelScreenshot)
									]
								]

								+SVerticalBox::Slot()
								.Padding(0.0,0.0,0.0,16.0)
								.FillHeight(1.0)
								[
									SNew(SBorder)
									.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Medium"))
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.FillHeight(1.0)
										[
											SNew(SScrollBox)
											+ SScrollBox::Slot()
											.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
											[
												SAssignNew(ChallengeDescription, SRichTextBlock)
												.Text(NSLOCTEXT("SUTChallengePanel", "Description", "This is the description"))	// Change me to a delegate call
												.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
												.Justification(ETextJustify::Center)
												.DecoratorStyleSet(&SUTStyle::Get())
												.AutoWrapText(true)
											]
										]
										+SVerticalBox::Slot()
										.AutoHeight()
										[
											SNew(SBox).HeightOverride(64)
											[
												SNew(SVerticalBox)
												+SVerticalBox::Slot()
												.AutoHeight()
												[
													SNew(STextBlock)
													.Text(this,&SUTChallengePanel::GetCurrentScoreText)
													.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium.Bold")
													.ColorAndOpacity(FLinearColor(0.5f,0.5f,0.5f))

												]
												+SVerticalBox::Slot()
												.AutoHeight()
												[
													SNew(STextBlock)
													.Text(this,&SUTChallengePanel::GetCurrentChallengeData)
													.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small.Bold")
													.ColorAndOpacity(FLinearColor(0.5f,0.5f,0.5f))
												]
											
											]
										]
									]
								]
								+SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SBorder)
									.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.SuperDark"))
									[
										SNew(SVerticalBox)
										+SVerticalBox::Slot()
										.AutoHeight()
										[
											SNew(SHorizontalBox)
											+SHorizontalBox::Slot()
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											[
												SNew(STextBlock)
												.Text(NSLOCTEXT("SUTChallengePanel","DifficultyText","Select Difficulty"))
												.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Small.Bold")
											]
										]
										+SVerticalBox::Slot()
										.HAlign(HAlign_Fill)
										.AutoHeight()
										[
											SNew(SBox).HeightOverride(96)
											[
												SNew(SHorizontalBox)
												+SHorizontalBox::Slot()
												[
													SNew(SBorder)
													.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
													[
														SNew(SButton)
														.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
														.OnClicked(this, &SUTChallengePanel::StartClicked, 0)
														[
															SNew(SVerticalBox)
															+SVerticalBox::Slot()
															.AutoHeight()
															.HAlign(HAlign_Center)
															[
																SNew(STextBlock)
																.Text(NSLOCTEXT("SUTChallengePanel","Level_Easy","Easy"))
																.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium")

															]
															+SVerticalBox::Slot()
															.AutoHeight()
															.HAlign(HAlign_Center)
															[
																SNew(SHorizontalBox)
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarCompletedImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)
																	]
																]
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)
																	]
																]
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)
																	]
																]
															]
														]
													]
												]
												+SHorizontalBox::Slot()
												.Padding(3.0,0.0,3.0,0.0)
												[
													SNew(SBorder)
													.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
													[
														SNew(SButton)
														.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
														.OnClicked(this, &SUTChallengePanel::StartClicked, 1)
														[
															SNew(SVerticalBox)
															+SVerticalBox::Slot()
															.AutoHeight()
															.HAlign(HAlign_Center)
															[
																SNew(STextBlock)
																.Text(NSLOCTEXT("SUTChallengePanel","Level_Medium","Medium"))
																.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium")

															]
															+SVerticalBox::Slot()
															.AutoHeight()
															.HAlign(HAlign_Center)
															[
																SNew(SHorizontalBox)
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarCompletedImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)

																	]
																]
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarCompletedImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)
																	]
																]
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)
																	]
																]
															]
														]
													]										
												]
												+SHorizontalBox::Slot()
												[
													SNew(SBorder)
													.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
													[
														SNew(SButton)
														.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
														.OnClicked(this, &SUTChallengePanel::StartClicked, 2)
														[
															SNew(SVerticalBox)
															+SVerticalBox::Slot()
															.AutoHeight()
															.HAlign(HAlign_Center)
															[
																SNew(STextBlock)
																.Text(NSLOCTEXT("SUTChallengePanel","Level_Hard","Hard"))
																.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium")

															]
															+SVerticalBox::Slot()
															.AutoHeight()
															.HAlign(HAlign_Center)
															[
																SNew(SHorizontalBox)
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarCompletedImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)
																	]
																]
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarCompletedImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)
																	]
																]
																+SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SBox).WidthOverride(32).HeightOverride(32)
																	[
																		SNew(SImage)
																		.Image(this, &SUTChallengePanel::GetStarCompletedImage)
																		.ColorAndOpacity(this, &SUTChallengePanel::GetSelectMatchColor)
																	]
																]
															]
														]
													]										
												]
											]
										]
									]
								]
							]
						]
					]
				]
			]
		]
	];

	GenerateChallengeList();

}
const FSlateBrush* SUTChallengePanel::GetStarImage() const
{
	return SUTStyle::Get().GetBrush(SelectedStarStyle);
}

const FSlateBrush* SUTChallengePanel::GetStarCompletedImage() const

{
	return SUTStyle::Get().GetBrush(SelectedStarStyle_Completed);
}

void SUTChallengePanel::GenerateChallengeList()
{
	ChallengeBox->ClearChildren();
	LastReward = NAME_REWARD_None;

	if (ChallengeManager.IsValid())
	{
		if (PlayerOwner.IsValid())
		{
			if (PlayerOwner->ChallengeRevisionNumber < ChallengeManager->RevisionNumber)
			{
				PlayerOwner->ChallengeRevisionNumber = ChallengeManager->RevisionNumber;
				//PlayerOwner->SaveConfig();
			}
		}

		TArray<FName> Tags;

		// Sort the challenges...
		for (auto It = ChallengeManager->Challenges.CreateConstIterator(); It; ++It)
		{
			const FUTChallengeInfo Challenge = It.Value();
			FName ChallengeTag = It.Key();

			int32 RewardPosition = ChallengeManager->RewardTags.Find(Challenge.RewardTag);

			int32 Stars = PlayerOwner->GetChallengeStars(ChallengeTag);
			if (RewardStars.Contains(Challenge.RewardTag))
			{
				RewardStars[Challenge.RewardTag] = RewardStars[Challenge.RewardTag] + Stars;
			}
			else
			{
				RewardStars.Add(Challenge.RewardTag, Stars);
			}

			bool bAdded = false;
			for (int32 i = 0; i < Tags.Num(); i++)
			{
				int32 ChallengeRewardPosition = ChallengeManager->RewardTags.Find(ChallengeManager->Challenges[Tags[i]].RewardTag);

				if ( RewardPosition > ChallengeRewardPosition) 	
				{
					Tags.Insert(ChallengeTag, i);
					bAdded = true;
					break;
				}
			}

			if (!bAdded) Tags.Add(ChallengeTag);
		}
		
		SelectedChallenge = Tags.Num() > 0 ? Tags[0] : NAME_None;
		for (int32 i = 0; i < Tags.Num(); i++)
		{
			AddChallengeButton(Tags[i], ChallengeManager->Challenges[Tags[i]]);
		}
	}

	ChallengeClicked(SelectedChallenge);
}

void SUTChallengePanel::AddChallengeButton(FName ChallengeTag, const FUTChallengeInfo& Challenge)
{
	TSharedPtr<SUTButton> Button;

	if (Challenge.RewardTag != LastReward && ChallengeManager->RewardCaptions.Contains(Challenge.RewardTag))
	{
		LastReward = Challenge.RewardTag;
		int32 StarCount = RewardStars[Challenge.RewardTag];

		FText Caption = FText::Format(ChallengeManager->RewardCaptions[LastReward], FText::AsNumber(StarCount));

		ChallengeBox->AddSlot()
		.Padding(10.0,48.0,10.0,15.0)
		.AutoHeight()
		.HAlign(HAlign_Right)
		[
			SNew(SBox).HeightOverride(42)
			[
				SNew(STextBlock)
				.Text(Caption)
				.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium.Bold")
				.ShadowOffset(FVector2D(2.0f,2.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f,0.0f,0.0f,0.5))
				
			]
		];
	}

	FLinearColor StarColor = ChallengeManager->RewardInfo.Contains(LastReward) ? ChallengeManager->RewardInfo[LastReward].StarColor : FLinearColor(1.0,1.0,0.0,1.0);
	FName StarStyle = ChallengeManager->RewardInfo.Contains(LastReward) ? ChallengeManager->RewardInfo[LastReward].StarEmptyStyleTag: NAME_REWARDSTYLE_STAR;
	FName CompletedStarStyle = ChallengeManager->RewardInfo.Contains(LastReward) ? ChallengeManager->RewardInfo[LastReward].StarCompletedStyleTag: NAME_REWARDSTYLE_STAR_COMPLETED;

	ChallengeBox->AddSlot()
	.Padding(10.0,6.0,10.0,6.0)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		[
			SNew(SBox).WidthOverride(860).HeightOverride(96)
			[
				SAssignNew(Button, SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.HomePanel.Button")
				.IsToggleButton(true)
				.OnClicked(this, &SUTChallengePanel::ChallengeClicked, ChallengeTag)
				[
					SNew(SBorder)
					.BorderImage(SUTStyle::Get().GetBrush(Challenge.SlateUIImageName))
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							CreateCheck(ChallengeTag)
						]
						+SOverlay::Slot()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(70.0,0.0,0.0,0.0)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromString(Challenge.Title))
								.TextStyle(SUTStyle::Get(),"UT.Font.NormalText.Medium.Bold")
								.ShadowOffset(FVector2D(2.0f,2.0f))
								.ShadowColorAndOpacity(FLinearColor(0.0f,0.0f,0.0f,0.5))
							]
						]
						+SOverlay::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.HAlign(HAlign_Right)
							[
								CreateStars(ChallengeTag,StarColor, StarStyle, CompletedStarStyle)
							]
						]
					]
				]
			]
		]
	];

	ButtonMap.Add(ChallengeTag, Button);



}

TSharedRef<SWidget> SUTChallengePanel::CreateCheck(FName ChallengeTag)
{
	int32 NoStars = PlayerOwner->GetChallengeStars(ChallengeTag);
	if (NoStars > 0)
	{
		return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(0.0,0.0,2.0,0.0)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox).WidthOverride(64).HeightOverride(64)
				[
					SNew(SImage)
					.Image(SUTStyle::Get().GetBrush("UT.Icon.Checkmark"))
				]
			]
		];
	}

	return SNew(SCanvas);
}

TSharedRef<SWidget> SUTChallengePanel::CreateStars(FName ChallengeTag, FLinearColor StarColor, FName StarStyle, FName CompletedStarStyle)
{
	int32 NoStars = PlayerOwner->GetChallengeStars(ChallengeTag);
	TSharedPtr<SHorizontalBox> Box;
	SAssignNew(Box, SHorizontalBox);

	for (int32 i=0; i < 3; i++)
	{
		if (i < NoStars)
		{
			Box->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0.0,0.0,2.0,0.0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox).WidthOverride(32).HeightOverride(32)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush(CompletedStarStyle))
						.ColorAndOpacity(FSlateColor(StarColor))
					]
				]
			];
		}
		else
		{
			Box->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0.0,0.0,2.0,0.0)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox).WidthOverride(32).HeightOverride(32)
					[
						SNew(SImage)
						.Image(SUTStyle::Get().GetBrush(StarStyle))
						.ColorAndOpacity(FSlateColor(StarColor))
					]
				]
			];
		}
	}

	return Box.ToSharedRef();
}

FText SUTChallengePanel::GetYourScoreText() const
{
	return FText::Format(NSLOCTEXT("SUTChallengePanel","StarTotalFormat","You have earned {0} total stars"), FText::AsNumber(PlayerOwner->GetTotalChallengeStars()));
}

FText SUTChallengePanel::GetCurrentScoreText() const
{
	return FText::Format(NSLOCTEXT("SUTChallengePanel","StarsForChallengeFormat","You have earned {0} stars for this challenge"), FText::AsNumber(PlayerOwner->GetChallengeStars(SelectedChallenge)));
}

FText SUTChallengePanel::GetCurrentChallengeData() const
{
	return FText::Format(NSLOCTEXT("SUTChallengePanel","DateForChallengeFormat","Last Completed: {0}"), FText::FromString(PlayerOwner->GetChallengeDate(SelectedChallenge)));
}


FReply SUTChallengePanel::ChallengeClicked(FName ChallengeTag)
{
	if (ChallengeManager.IsValid() && ChallengeManager->Challenges.Contains(ChallengeTag))
	{
		FString Description = ChallengeManager->Challenges[ChallengeTag].Description;
		FString Map = ChallengeManager->Challenges[ChallengeTag].Map;

		FName RewardTag = ChallengeManager->Challenges[ChallengeTag].RewardTag;
		
		SelectedStarStyle = ChallengeManager->RewardInfo.Contains(RewardTag) ? ChallengeManager->RewardInfo[RewardTag].StarEmptyStyleTag : NAME_REWARDSTYLE_STAR;
		SelectedStarStyle_Completed  = ChallengeManager->RewardInfo.Contains(RewardTag) ? ChallengeManager->RewardInfo[RewardTag].StarCompletedStyleTag : NAME_REWARDSTYLE_STAR_COMPLETED;

		SelectedChallenge = ChallengeTag;

		for (auto It = ButtonMap.CreateConstIterator(); It; ++It)
		{
			TSharedPtr<SUTButton> Button = It.Value();
			FName Key = It.Key();
			if (Key == ChallengeTag)
			{
				Button->BePressed();
			}
			else
			{
				Button->UnPressed();
			}
		}

		bool bReset = true;

		TArray<FAssetData> MapAssets;
		GetAllAssetData(UWorld::StaticClass(), MapAssets, false);
		for (const FAssetData& Asset : MapAssets)
		{
			FString MapPackageName = Asset.PackageName.ToString();
			if (MapPackageName == Map)
			{
				const FString* Screenshot = Asset.TagsAndValues.Find(NAME_MapInfo_ScreenshotReference);
				const FString* MapDescription = Asset.TagsAndValues.Find(NAME_MapInfo_Description);

				if (MapDescription != NULL)
				{
					Description = Description + TEXT("\n\n<UT.Font.NormalText.Small.Bold>") + *MapDescription + TEXT("</>\n");
				}

				if (Screenshot != NULL)
				{
					LevelShot = LoadObject<UTexture2D>(nullptr, **Screenshot);
					if (LevelShot)
					{
						*LevelScreenshot = FSlateDynamicImageBrush(LevelShot, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
					}
					else
					{
						*LevelScreenshot = FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
					}
					bReset = false;
					break;
				}
			}
		}

		if (bReset )
		{
			*LevelScreenshot = FSlateDynamicImageBrush(Cast<UUTGameEngine>(GEngine)->DefaultLevelScreenshot, LevelScreenshot->ImageSize, LevelScreenshot->GetResourceName());
		}

		ChallengeDescription->SetText(FText::FromString(Description));


	}

	return FReply::Handled();
}

FReply SUTChallengePanel::StartClicked(int32 Difficulty)
{

	if (PlayerOwner->IsLoggedIn())
	{
		if (Difficulty > 0)
		{
			int32 NumStars = PlayerOwner->GetTotalChallengeStars();
			if (NumStars < 10)
			{
				const FUTChallengeInfo Challenge = ChallengeManager->Challenges[SelectedChallenge];
				if (Challenge.PlayerTeamSize > 0)
				{
					PendingDifficulty = Difficulty;
					PlayerOwner->ShowMessage(NSLOCTEXT("SUTChallengePanel", "WeakRosterWarningTitle", "WARNING"), NSLOCTEXT("SUTChallengePanel", "WeakRosterWarning", "Your current roster of teammates may not be strong enough to take on this challenge at this skill level.  You will unlock roster upgrades for every 5 stars earned.  Do you wish to continue?"), UTDIALOG_BUTTON_YES | UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUTChallengePanel::WarningResult));
					return FReply::Handled();
				}
			}
		}
		StartChallenge(Difficulty);
	}
	else
	{
		PendingDifficulty = Difficulty;
		PlayerOwner->ShowMessage(NSLOCTEXT("SUTChallengePanel", "NotLoggedInWarningTitle", "WARNING"), NSLOCTEXT("SUTChallengePanel", "NotLoggedInWarning", "You are not logged in.  Any progress you make on this challenge will not be saved.  Do you wish to continue?"), UTDIALOG_BUTTON_YES | UTDIALOG_BUTTON_NO, FDialogResultDelegate::CreateSP(this, &SUTChallengePanel::WarningResult));
	}

	return FReply::Handled();
}

void SUTChallengePanel::WarningResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_YES)
	{
		StartChallenge(PendingDifficulty);
	}
}

void SUTChallengePanel::StartChallenge(int32 Difficulty)
{

	if (ChallengeManager.IsValid() && ChallengeManager->Challenges.Contains(SelectedChallenge))
	{
		const FUTChallengeInfo Challenge = ChallengeManager->Challenges[SelectedChallenge];

		// Kill any existing Dedicated servers
		if (PlayerOwner->DedicatedServerProcessHandle.IsValid())
		{
			FPlatformProcess::TerminateProc(PlayerOwner->DedicatedServerProcessHandle,true);
			PlayerOwner->DedicatedServerProcessHandle.Reset();
		}

		if (FUTAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> ParamArray;
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Challenge"), SelectedChallenge.ToString()));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("Difficulty"), Difficulty));
			FUTAnalytics::GetProvider().RecordEvent( TEXT("StartChallenge"), ParamArray );
		}

		FString Options = FString::Printf(TEXT("%s%s?Challenge=%s?ChallengeDiff=%i"), *Challenge.Map, *Challenge.GameURL, *SelectedChallenge.ToString(), Difficulty);
		ConsoleCommand(TEXT("Open ") + Options);
	}
		
}

FReply SUTChallengePanel::CustomClicked()
{
	TSharedPtr<SUWindowsMainMenu> MainMenu = StaticCastSharedPtr<SUWindowsMainMenu>(PlayerOwner->GetCurrentMenu());
	if (MainMenu.IsValid())
	{
		MainMenu->ShowCustomGamePanel();
	}

	return FReply::Handled();
}

void SUTChallengePanel::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// If the challenge revision has changed, rebuild the list.
	if (ChallengeManager.IsValid() && ChallengeManager->RevisionNumber != LastChallengeRevisionNumber)
	{
		LastChallengeRevisionNumber = ChallengeManager->RevisionNumber;
		GenerateChallengeList();
	}
}

FSlateColor SUTChallengePanel::GetSelectMatchColor() const
{
	const FUTChallengeInfo Challenge = ChallengeManager->Challenges[SelectedChallenge];
	FLinearColor StarColor = ChallengeManager->RewardInfo.Contains(Challenge.RewardTag) ? ChallengeManager->RewardInfo[Challenge.RewardTag].StarColor : FLinearColor(1.0,1.0,0.0,1.0);
	return FSlateColor(StarColor);
}

#endif