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
#include "UTReplicatedMapVoteInfo.h"

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
	if (MapBox.IsValid() && GameState.IsValid())
	{
		MapBox->ClearChildren();

		if (GameState->VoteTimer > 0 && GameState->VoteTimer <= 10)
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

		TSharedPtr<SGridPanel> MapPanel;
		MapBox->AddSlot()
		[
			SAssignNew(MapPanel, SGridPanel)
		];

		VoteButtons.Empty();
		GameState->SortVotes();
		LastVoteCount = GameState->MapVoteList.Num();

		int32 NoColumns = FMath::Min<int32>(GameState->MapVoteList.Num(), 6);
		for (int32 i=0; i< GameState->MapVoteList.Num(); i++)
		{
			int32 Row = i / NoColumns;
			int32 Col = i % NoColumns;

			TWeakObjectPtr<AUTReplicatedMapVoteInfo> MapVoteInfo = GameState->MapVoteList[i];
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
									+SVerticalBox::Slot().FillHeight(1.0).VAlign(VAlign_Bottom).HAlign(HAlign_Right)
									[
										SNew(STextBlock)
										.Text(FText::AsNumber(MapVoteInfo->VoteCount))
										.TextStyle(SUWindowsStyle::Get(),"UT.Dialog.BodyTextStyle")
									]

								]
							]
						]
						+SVerticalBox::Slot()
						.HAlign(HAlign_Center)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(MapVoteInfo->MapTitle))
							.TextStyle(SUWindowsStyle::Get(),"UT.Hub.MapsText")
							.ColorAndOpacity(FLinearColor::Black)
						]
					]
				]
			];

			VoteButtons.Add( FVoteButton(MapVoteInfo, ImageWidget) );
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
					VoteButtons[i].MapVoteInfo->MapBrush = new FSlateDynamicImageBrush(Tex, FVector2D(256.0, 128.0), NAME_None);
					VoteButtons[i].MapImage->SetImage(VoteButtons[i].MapVoteInfo->MapBrush);
				}
			}
		}
	}
}



FReply SUWMapVoteDialog::OnMapClick(TWeakObjectPtr<AUTReplicatedMapVoteInfo> MapInfo)
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


#endif