// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class SUWindowsMainMenu;
class SUTBorder;

class UNREALTOURNAMENT_API SUHomePanel : public SUWPanel
{
	virtual void ConstructPanel(FVector2D ViewportSize);
	virtual TSharedRef<SWidget> BuildHomePanel();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

protected:
	FReply BasicTraining_Click();
	FReply QuickMatch_DM_Click();
	FReply QuickMatch_CTF_Click();
	FReply OfflineAction_Click();
	FReply FindAMatch_Click();
	FReply FragCenter_Click();
	FReply RecentMatches_Click();
	FReply WatchLive_Click();
	FReply TrainingVideos_Click();

	FReply ViewTournament(int32 Which);

	virtual FLinearColor GetFadeColor() const;
	virtual FSlateColor GetFadeBKColor() const;

	TSharedPtr<SVerticalBox> AnnouncementBox;

	virtual void BuildAnnouncement();

	float AnnouncmentTimer;
	float AnnouncmentFadeTimer;

	EVisibility ShowNewChallengeImage() const;

	FSlateColor GetFragCenterWatchNowColorAndOpacity() const;

	TSharedPtr<SUTBorder> TrainingWidget;

};

#endif