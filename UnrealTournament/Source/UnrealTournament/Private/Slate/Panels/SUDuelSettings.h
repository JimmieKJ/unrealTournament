// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "SNumericEntryBox.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMatchPanel.h"
#include "UTLobbyMatchInfo.h"
#include "UTLocalPlayer.h"

#if !UE_SERVER


class SUDuelSettings : public SULobbyGameSettingsPanel
{
public:
	virtual TSharedRef<SWidget> ConstructContents();

	/**
	 *	Called when this widget needs to refresh it's options.  This is only called on clients that are not the match owner.
	 **/
	virtual void RefreshOptions();

	/**
	 *	Called to commit the local set of options.  This will replicate from the host to the server then out to the clients
	 **/
	virtual void CommitOptions();

protected:

	int32 TimeLimit;
	int32 GoalScore;


	TSharedPtr<SNumericEntryBox<int32>> ebGoalScore;
	TSharedPtr<STextBlock> tbGoalScore;

	TSharedPtr<SNumericEntryBox<int32>> ebTimeLimit;
	TSharedPtr<STextBlock> tbTimeLimit;

	TSharedPtr<STextBlock> tbMapName;

	void SetGoalScore(int32 inGoalScore);
	void SetTimeLimit(int32 inTimeLimit);

	TSharedRef<SWidget> AddTextOption(FText Caption, FString DefaultValue);
	TSharedRef<SWidget> AddIntOption(FText Caption, FString DefaultValue);

	TOptional<int> Edit_GetTimeLimit() const;
	TOptional<int> Edit_GetGoalScore() const;
	void Edit_SetTimeLimit(int inTimeLimit);
	void Edit_SetGoalScore(int inGoalScore);
	void Edit_TimeLimitCommit(int NewTimeLimit, ETextCommit::Type CommitType);
	void Edit_GoalScoreCommit(int NewTimeLimit, ETextCommit::Type CommitType);

	TSharedRef<SWidget> CreateGoalScoreWidget();
	TSharedRef<SWidget> CreateTimeLimitWidget();
	TSharedRef<SWidget> CreateMapsWidget();

	void OnMapListChanged(TSharedPtr<FAllowedMapData> SelectedItem, ESelectInfo::Type SelectInfo);

	TSharedRef<ITableRow> GenerateMapListWidget(TSharedPtr<FAllowedMapData> InItem, const TSharedRef<STableViewBase>& OwningList);
	FString GetMapText() const;

	FText GetGoalScore() const;
	FText GetTimeLimit() const;
};

#endif