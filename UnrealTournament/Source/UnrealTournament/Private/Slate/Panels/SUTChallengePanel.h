// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "../Base/SUTPanelBase.h"

#if !UE_SERVER

class UUTChallengeManager;
class SUTButton;

static const FName NAME_REWARD_None(TEXT("REWARD_None"));

class SUTBorder;

class UNREALTOURNAMENT_API SUTChallengePanel : public SUTPanelBase, public FGCObject
{
public:
	virtual ~SUTChallengePanel();
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	
	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);
	virtual void OnHidePanel();

private:

	virtual void AnimEnd();
	TSharedPtr<SUTBorder> AnimWidget;

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

	EChallengeFilterType::Type ChallengeFilter;

	TArray<TSharedPtr<SUTButton>> ChallengeTabs;
	virtual FReply TabChanged(int32 Index);

	TSharedPtr<SVerticalBox> GoBox;
	void BuildGoBox();
	FSlateColor GetTabColor(EChallengeFilterType::Type TargetFilter) const;
};

#endif
