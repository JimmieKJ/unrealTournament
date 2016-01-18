// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "../Base/SUTDialogBase.h"
#include "../Widgets/SUTTabButton.h"
#include "../Widgets/SUTComboButton.h"
#include "../Widgets/SUTButton.h"
#include "UTLobbyMatchInfo.h"
#include "UTGameState.h"
#include "UTAudioSettings.h"
#include "UTLevelSummary.h"
#include "UTReplicatedGameRuleset.h"
#include "../Panels/SUTCreateGamePanel.h"
#include "UTGameEngine.h"

#if !UE_SERVER

class SUTPlayerListPanel;
class SUTTextChatPanel;

const int32 MAP_COLUMNS = 3;

class UNREALTOURNAMENT_API SUTMapVoteDialog : public SUTDialogBase, public FGCObject
{
	SLATE_BEGIN_ARGS(SUTMapVoteDialog)
	: _DialogTitle(NSLOCTEXT("SUTMapVoteDialog", "Title", "CHOOSE YOUR NEXT MAP..."))
	, _DialogSize(FVector2D(1800, 1000))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_OK)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(TWeakObjectPtr<class AUTGameState>, GameState)
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_ARGUMENT(uint16, ButtonMask)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void OnDialogClosed() override;

	TWeakObjectPtr<class AUTGameState> GameState;

protected:

	TSharedPtr<SUTPlayerListPanel> PlayerListPanel;
	TSharedPtr<SUTTextChatPanel> TextChatPanel;

	FSlateDynamicImageBrush* DefaultLevelScreenshot;

	struct FVoteButton
	{
		UTexture2D* MapTexture;
		TWeakObjectPtr<AUTReplicatedMapInfo> MapVoteInfo;
		TSharedPtr<SImage> MapImage;
		TSharedPtr<SBorder> BorderWidget;
		TSharedPtr<STextBlock> MapTitle;
		TSharedPtr<STextBlock> VoteCountText;
		TSharedPtr<SUTButton> VoteButton;

		FVoteButton()
		{
			MapTexture = NULL;
			MapVoteInfo.Reset();
			MapImage.Reset();
		}

		FVoteButton(UTexture2D* inMapTexture, TWeakObjectPtr<AUTReplicatedMapInfo> inMapVoteInfo, TSharedPtr<SUTButton> inVoteButton, TSharedPtr<SImage> inMapImage, TSharedPtr<STextBlock> inMapTitle, TSharedPtr<STextBlock> inVoteCountText, TSharedPtr<SBorder> inBorderWidget)
			: MapTexture(inMapTexture)
			, MapVoteInfo(inMapVoteInfo)
			, MapImage(inMapImage)
			, BorderWidget(inBorderWidget)
			, MapTitle(inMapTitle)
			, VoteCountText(inVoteCountText)
			, VoteButton(inVoteButton)
		{
		}

	};

	int32 NumMapInfos;

	TArray<FVoteButton> LeadingVoteButtons;
	TArray<FVoteButton> VoteButtons;
	TSharedPtr<SScrollBox> MapBox;
	void BuildMapList();

	void BuildTopVotes();
	void UpdateTopVotes();
	void BuildAllVotes();

	TSharedPtr<SGridPanel> TopPanel;
	TSharedPtr<SGridPanel> MapPanel;
	
	void TextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);
	void LeaderTextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);
	FReply OnLeadingMapClick(int32 ButtonIndex);
	FReply OnMapClick(TWeakObjectPtr<AUTReplicatedMapInfo> MapInfo);
	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	virtual TSharedRef<class SWidget> BuildCustomButtonBar();
	FText GetClockTime() const;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		for (int32 i = 0; i < VoteButtons.Num(); i++)
		{
			if (VoteButtons[i].MapTexture != NULL)
			{
				Collector.AddReferencedObject(VoteButtons[i].MapTexture);
			}
		}
	}

	virtual FReply OnButtonClick(uint16 ButtonID) override;

};

#endif