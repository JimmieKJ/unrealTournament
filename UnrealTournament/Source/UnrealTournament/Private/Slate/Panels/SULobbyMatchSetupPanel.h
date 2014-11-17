// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUMatchPanel.h"
#include "UTLobbyMatchInfo.h"
#include "UTLocalPlayer.h"
#include "TAttributeProperty.h"

#if !UE_SERVER

class FGameModeListData
{
public: 
	FString DisplayText;
	TWeakObjectPtr<UClass> GameClass;

	FGameModeListData(FString inDisplayText, UClass* inGameClass)
		: DisplayText(inDisplayText)
		, GameClass(inGameClass)
	{
	};

	static TSharedRef<FGameModeListData> Make( FString inDisplayText, UClass* inGameClass)
	{
		return MakeShareable( new FGameModeListData( inDisplayText, inGameClass) );
	}
};


class SULobbyMatchSetupPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SULobbyMatchSetupPanel)
		: _bIsHost(false)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_ARGUMENT( TWeakObjectPtr<AUTLobbyMatchInfo>, MatchInfo )
		SLATE_ARGUMENT( bool, bIsHost )

	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

protected:

	// Will be true if this is the host's setup window
	bool bIsHost;

	UPROPERTY()
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;

	UPROPERTY()
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	TSharedPtr<SVerticalBox> SettingsPanel;

	// Holds the display name for a list of game types
	TArray<TSharedPtr<FAllowedGameModeData>> AvailableGameModes;

	TSharedPtr<STextBlock> CurrentGameMode;

	TSharedRef<SWidget> GenerateGameModeListWidget(TSharedPtr<FAllowedGameModeData> InItem);

	TSharedPtr<FAllowedGameModeData> CurrentGameModeData;
	void OnGameModeChanged(TSharedPtr<FAllowedGameModeData> NewSelection, ESelectInfo::Type SelectInfo);

	TSharedRef<SWidget> BuildGameModeWidget();

	FString GetGameModeText() const;
	FString GameModeDisplayName;

	/**
	 *	Given a default GameMode object, create the game mode panel
	 **/
	void ChangeGameModePanel(AUTGameMode* DefaultGameMode);

	FOnMatchInfoGameModeChanged OnMatchInfoGameModeChangedDelegate;
	void OnMatchGameModeChanged();

	TSharedPtr<SWidget> Settings;

};

#endif