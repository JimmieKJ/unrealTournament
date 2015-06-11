// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#if !UE_SERVER

#include "SlateBasics.h"
#include "STableRow.h"
#include "AssetData.h"

class UNREALTOURNAMENT_API SUWBotConfigDialog : public SUWDialog
{
public:
	SLATE_BEGIN_ARGS(SUWBotConfigDialog)
		: _DialogTitle(NSLOCTEXT("SUWCreateGamePanel", "ConfigureBots", "Configure Bots"))
		, _DialogSize(FVector2D(800, 900))
		, _bDialogSizeIsRelative(false)
		, _DialogPosition(FVector2D(0.5f, 0.5f))
		, _DialogAnchorPoint(FVector2D(0.5f, 0.5f))
	{}
	SLATE_ARGUMENT(FText, DialogTitle)
	SLATE_ARGUMENT(FVector2D, DialogSize)
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)
	SLATE_ARGUMENT(FVector2D, DialogPosition)
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)
	SLATE_ARGUMENT(TSubclassOf<AUTGameMode>, GameClass)
	SLATE_ARGUMENT(TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner)
	SLATE_ARGUMENT(int32, NumBots) // number of bots that will be added to the game we're setting up
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSubclassOf<AUTGameMode> GameClass;
	int32 MaxSelectedBots;

	TArray< TSharedPtr<FAssetData> > AvailableBots, IncludedBots;

	TSharedPtr< SListView< TSharedPtr<FAssetData> > > BotAvailableList, BotIncludeList;

	TSharedRef<ITableRow> GenerateBotListRow(TSharedPtr<FAssetData> BotEntry, const TSharedRef<STableViewBase>& OwningList);
	FReply AddBot();
	FReply RemoveBot();
	virtual FReply OnButtonClick(uint16 ButtonID);
};

#endif