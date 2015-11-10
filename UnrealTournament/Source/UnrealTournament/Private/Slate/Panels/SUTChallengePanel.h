// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../SUWPanel.h"

#if !UE_SERVER

class UUTChallengeManager;
class SUTButton;

static const FName NAME_REWARD_None(TEXT("REWARD_None"));


class UNREALTOURNAMENT_API SUTChallengePanel : public SUWPanel, public FGCObject
{
public:
	virtual ~SUTChallengePanel();
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	
private:
	virtual void ConstructPanel(FVector2D ViewportSize);	
	bool bLoadedChallengesFromMCP;

protected:
	int32 PendingDifficulty;
	FName SelectedChallenge;
	TSharedPtr<SVerticalBox> ChallengeBox;
	TSharedPtr<SRichTextBlock> ChallengeDescription;
	TWeakObjectPtr<UUTChallengeManager> ChallengeManager;

	TMap<FName, TSharedPtr<SUTButton>> ButtonMap;

	FText GetYourScoreText() const;
	FText GetCurrentScoreText() const;
	FText GetCurrentChallengeData() const;

	TSharedRef<SWidget> CreateCheck(FName ChallengeTag);
	TSharedRef<SWidget> CreateStars(FName ChallengeTag, FLinearColor StarColor, FName StarStyle, FName CompletedStarStyle);

	virtual void GenerateChallengeList();
	virtual FReply ChallengeClicked(FName ChallengeTag);
	virtual FReply StartClicked(int32 Difficulty);
	virtual FReply CustomClicked();

	virtual void StartChallenge(int32 Difficulty);
	void WarningResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	FSlateDynamicImageBrush* LevelScreenshot;
	UTexture2D* LevelShot;

	void  AddChallengeButton(FName ChallengeTag, const FUTChallengeInfo& Challenge);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(LevelShot);
	}

	FName LastReward;

	TMap<FName, int32> RewardStars;

	FSlateColor GetSelectMatchColor() const;
	int32 LastChallengeRevisionNumber;

	FName SelectedStarStyle;
	FName SelectedStarStyle_Completed;

	const FSlateBrush* GetStarImage() const;
	const FSlateBrush* GetStarCompletedImage() const;

	int32 ChallengeFilterIndex;

	TArray<TSharedPtr<SUTButton>> ChallengeTabs;
	virtual FReply TabChanged(int32 Index);

	TSharedPtr<SVerticalBox> GoBox;
	void BuildGoBox();
};

#endif
