// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "SUWDialog.h"
#include "Widgets/SUTTabButton.h"
#include "Widgets/SUTComboButton.h"
#include "UTLobbyMatchInfo.h"
#include "UTGameState.h"
#include "UTAudioSettings.h"
#include "UTLevelSummary.h"
#include "UTReplicatedGameRuleset.h"
#include "Panels/SUWCreateGamePanel.h"
#include "UTGameEngine.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUWMapVoteDialog : public SUWDialog
{
	SLATE_BEGIN_ARGS(SUWMapVoteDialog)
	: _DialogTitle(NSLOCTEXT("SUWMapVoteDialog", "Title", "CHOOSE YOUR NEXT MAP..."))
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

	TWeakObjectPtr<class AUTGameState> GameState;

protected:

	FSlateDynamicImageBrush* DefaultLevelScreenshot;

	struct FVoteButton
	{
		TWeakObjectPtr<AUTReplicatedMapVoteInfo> MapVoteInfo;
		TSharedPtr<SImage> MapImage;

		FVoteButton()
		{
			MapVoteInfo.Reset();
			MapImage.Reset();
		}

		FVoteButton(TWeakObjectPtr<AUTReplicatedMapVoteInfo> inMapVoteInfo, TSharedPtr<SImage> inMapImage)
			: MapVoteInfo(inMapVoteInfo)
			, MapImage(inMapImage)
		{
		}

	};

	int32 LastVoteCount;

	TArray<FVoteButton> VoteButtons;
	TSharedPtr<SScrollBox> MapBox;
	void BuildMapList();
	
	void TextureLoadComplete(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result);
	FReply OnMapClick(TWeakObjectPtr<AUTReplicatedMapVoteInfo> MapInfo);
	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	virtual TSharedRef<class SWidget> BuildCustomButtonBar();
	FText GetClockTime() const;

};

#endif