// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../SUWPanel.h"

#if !UE_SERVER

class UUTChallengeManager;
class SUTButton;

class UNREALTOURNAMENT_API SUTChallengePanel : public SUWPanel
{
private:
	virtual void ConstructPanel(FVector2D ViewportSize);	

protected:
	FName SelectedChallenge;
	TSharedPtr<SVerticalBox> ChallengeBox;
	TSharedPtr<SRichTextBlock> ChallengeDescription;
	TWeakObjectPtr<UUTChallengeManager> ChallengeManager;

	TMap<FName, TSharedPtr<SUTButton>> ButtonMap;

	FText GetYourScoreText() const;
	FText GetCurrentScoreText() const;
	FText GetCurrentChallengeData() const;

	TSharedRef<SWidget> CreateCheck(FName ChallengeTag);
	TSharedRef<SWidget> CreateStars(FName ChallengeTag);

	virtual void GenerateChallengeList();
	virtual FReply ChallengeClicked(FName ChallengeTag);
	virtual FReply StartClicked(int32 Difficulty);
	virtual FReply CustomClicked();
};

#endif
