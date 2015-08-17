// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

class SUWindowsMainMenu;

class UNREALTOURNAMENT_API SUHomePanel : public SUWPanel
{
	virtual void ConstructPanel(FVector2D ViewportSize);
	virtual TSharedRef<SWidget> BuildHomePanel();

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

};

#endif