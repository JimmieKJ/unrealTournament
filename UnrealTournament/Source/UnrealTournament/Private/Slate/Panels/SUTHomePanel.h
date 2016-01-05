// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../Base/SUTPanelBase.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class SUTMainMenu;
class SUTBorder;

class UNREALTOURNAMENT_API SUTHomePanel : public SUTPanelBase
{
	virtual void ConstructPanel(FVector2D ViewportSize);
	virtual TSharedRef<SWidget> BuildHomePanel();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	virtual bool ShouldShowBackButton()
	{
		return false;
	}

protected:

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);
	virtual void OnHidePanel();


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

	TSharedPtr<SUTBorder> AnimWidget;

	virtual void AnimEnd();

};

#endif