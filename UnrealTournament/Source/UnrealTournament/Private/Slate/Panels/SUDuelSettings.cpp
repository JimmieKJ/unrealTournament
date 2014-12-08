
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMidGameInfoPanel.h"
#include "SULobbyGameSettingsPanel.h"
#include "SUDuelSettings.h"
#include "../Widgets/SUTButton.h"


#if !UE_SERVER

TSharedRef<SWidget> SUDuelSettings::ConstructContents()
{
	return SNew(SVerticalBox)
	+SVerticalBox::Slot()
	.HAlign(HAlign_Fill)
	.AutoHeight()
	[
		// First side is config

		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.MaxWidth(300)
		[
			SNew(SVerticalBox)

			// GoalScore

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			.Padding(0.0f,0.0f,0.0f,5.0f)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(5.0f,0.0f,0.0f,0.0f)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("GameSettings","GoalScore","Kill Limit:"))
						.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
					]
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(0.0f,0.0f,5.0f,0.0f)
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							CreateGoalScoreWidget()
						]
						+SHorizontalBox::Slot()
						.Padding(5.0,0.0,0.0,0.0)
						.AutoWidth()
						[
							SNew(SBox)
							.WidthOverride(50)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("Generic","Frags","Frags"))
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.SmallText")
							]
						]
					]
				]
			]

			// Time Limit

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			.Padding(0.0f,0.0f,0.0f,5.0f)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(5.0f,0.0f,0.0f,0.0f)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("GameSettings","TimeLimit","TimeLimit:"))
						.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
					]
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.Padding(0.0f,0.0f,5.0f,0.0f)
					.HAlign(HAlign_Right)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							CreateTimeLimitWidget()
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(5.0,0.0,0.0,0.0)
						[
							SNew(SBox)
							.WidthOverride(50)
							[
								SNew(STextBlock)
								.Text(NSLOCTEXT("Generic","Minutes","Mins"))
								.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.SmallText")
							]
						]
					]
				]
			]
		]
		+SHorizontalBox::Slot()
		.FillWidth(1.0)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				SNew(SBox)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.Padding(5.0f,0.0f,0.0f,0.0f)
						.HAlign(HAlign_Left)
						[
							SNew(STextBlock)
							.Text(NSLOCTEXT("GameSettings","CurrentMap","- Map to Play -"))
							.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText")
						]
					]
				]
			]
			+SVerticalBox::Slot()
			.MaxHeight(150)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SImage)
						.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.Backdrop"))
						.ColorAndOpacity(FLinearColor(1.0,1.0,1.0,0.5))
					]
					+SOverlay::Slot()
					[
						CreateMapsWidget()
					]
				]
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(200)
			.HeightOverride(107)							
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("Testing.TestMapShot"))
			]
		]
	];
}

TSharedRef<SWidget> SUDuelSettings::CreateMapsWidget()
{
	if (bIsHost)
	{
		TSharedPtr<FAllowedMapData> MapData =  MatchInfo->HostAvailableMaps[0];
		return SNew(SBox)
		.WidthOverride(300)
		[
			SNew( SListView< TSharedPtr<FAllowedMapData> > )
			// List view items are this tall
			.ItemHeight(24)
			// Tell the list view where to get its source data
			.ListItemsSource(&MatchInfo->HostAvailableMaps)
			// When the list view needs to generate a widget for some data item, use this method
			.OnGenerateRow(this, &SUDuelSettings::GenerateMapListWidget)
			.OnSelectionChanged(this, &SUDuelSettings::OnMapListChanged)
			.SelectionMode(ESelectionMode::Single)
		];

	}
	else
	{
		return SNew(STextBlock)
		.Text(this, &SUDuelSettings::GetMapText)
		.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.NormalText");
	}
}

void SUDuelSettings::OnMapListChanged(TSharedPtr<FAllowedMapData> SelectedItem, ESelectInfo::Type SelectInfo)
{
	if (MatchInfo.IsValid() && bIsHost)
	{
		MatchInfo->SetMatchMap(SelectedItem->MapName);
	}
}

FString SUDuelSettings::GetMapText() const
{
	if (MatchInfo.IsValid())
	{
		return MatchInfo->MatchMap;
	}
	return TEXT("None");
}

TSharedRef<SWidget> SUDuelSettings::CreateGoalScoreWidget()
{
	if (bIsHost)
	{
		return SNew(SBox)
		.WidthOverride(100)
		[
			SAssignNew(ebGoalScore,SNumericEntryBox<int32>)
			.Value(this, &SUDuelSettings::Edit_GetGoalScore)
			.OnValueChanged(this, &SUDuelSettings::Edit_SetGoalScore)
			.OnValueCommitted(this, &SUDuelSettings::Edit_GoalScoreCommit)
			.AllowSpin(true)
			.Delta(1)
			.MinValue(0)
			.MaxValue(1000)
			.MinSliderValue(0)
			.MaxSliderValue(1000)
		];
	}
	else
	{	
		return SNew(SBox)
		.WidthOverride(100)
		[
			SAssignNew(tbGoalScore,STextBlock)
			.Text(this,&SUDuelSettings::GetGoalScore)
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.SmallText")
		];
	}
}
TSharedRef<SWidget> SUDuelSettings::CreateTimeLimitWidget()
{
	if (bIsHost)
	{
		return SNew(SBox)
		.WidthOverride(100)
		[
			SAssignNew(ebTimeLimit,SNumericEntryBox<int32>)
			.Value(this, &SUDuelSettings::Edit_GetTimeLimit)
			.OnValueChanged(this, &SUDuelSettings::Edit_SetTimeLimit)
			.OnValueCommitted(this, &SUDuelSettings::Edit_TimeLimitCommit)
			.AllowSpin(true)
			.Delta(1)
			.MinValue(0)
			.MaxValue(1000)
			.MinSliderValue(0)
			.MaxSliderValue(1000)
		];
	}
	else
	{
		return SNew(SBox)
		.WidthOverride(100)
		[
			SAssignNew(tbTimeLimit,STextBlock)
			.Text(this,&SUDuelSettings::GetTimeLimit)
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.SmallText")
		];
	}
}


void SUDuelSettings::RefreshOptions()
{
	if (MatchInfo.IsValid())
	{
		TArray<FString> Options;
		MatchInfo->MatchOptions.ParseIntoArray(&Options, TEXT("?"),true);
		for (int32 i=0;i<Options.Num(); i++)
		{
			FString OptName;
			FString Value;
			Options[i].Split(TEXT("="), &OptName, &Value);
			OptName = OptName.ToLower();
			if (OptName == TEXT("goalscore")) SetGoalScore(FCString::Atoi(*Value));
			else if (OptName == TEXT("timelimit")) SetTimeLimit(FCString::Atoi(*Value));
		}
	}
}

void SUDuelSettings::CommitOptions()
{
	if (MatchInfo.IsValid())
	{
		FString Options = FString::Printf(TEXT("GoalScore=%i?TimeLimit=%i"), GoalScore, TimeLimit);
		MatchInfo->ServerMatchOptionsChanged(Options);
	}
}

FText SUDuelSettings::GetGoalScore() const
{
	return FText::AsNumber(GoalScore);
}

void SUDuelSettings::SetGoalScore(int32 inGoalScore)
{
	GoalScore = inGoalScore;
}

void SUDuelSettings::SetTimeLimit(int32 inTimeLimit) 
{
	TimeLimit = inTimeLimit;
}

FText SUDuelSettings::GetTimeLimit() const
{
	return FText::AsNumber(TimeLimit);
}


TOptional<int> SUDuelSettings::Edit_GetTimeLimit() const
{
	return TimeLimit;
}

TOptional<int> SUDuelSettings::Edit_GetGoalScore() const
{
	return GoalScore;
}

void SUDuelSettings::Edit_SetTimeLimit(int inTimeLimit)
{
	TimeLimit = inTimeLimit;
	CommitOptions();
}

void SUDuelSettings::Edit_SetGoalScore(int inGoalScore)
{
	GoalScore = inGoalScore;
	CommitOptions();
}

void SUDuelSettings::Edit_TimeLimitCommit(int NewTimeLimit, ETextCommit::Type CommitType)
{
	CommitOptions();
}

void SUDuelSettings::Edit_GoalScoreCommit(int NewTimeLimit, ETextCommit::Type CommitType)
{
	CommitOptions();
}

TSharedRef<ITableRow> SUDuelSettings::GenerateMapListWidget(TSharedPtr<FAllowedMapData> InItem, const TSharedRef<STableViewBase>& OwningList)
{
	return SNew(STableRow<TSharedPtr<FAllowedMapData>>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::White)
			.Text(FText::FromString(InItem.Get()->MapName))
			.TextStyle(SUWindowsStyle::Get(),"UWindows.Standard.SmallText")
		];
}


#endif