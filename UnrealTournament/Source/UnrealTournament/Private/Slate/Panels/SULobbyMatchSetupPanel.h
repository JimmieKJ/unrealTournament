// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
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

	// Pointer to the MatchInfo governing this panel
	UPROPERTY()
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	// Holds the current Settings data.
	TSharedPtr<SVerticalBox> GamePanel;
	
	// Holds the display name for a list of game types
	TArray<TSharedPtr<FAllowedGameModeData>> AvailableGameModes;

	// Holds the name of the current Game Mode if not the host
	TSharedPtr<STextBlock> CurrentGameMode;

	TSharedPtr<SCheckBox> JIPCheckBox;
	TSharedPtr<SHorizontalBox> MaxPlayersBox;

	// Used by the list code for the Game list
	TSharedRef<SWidget> GenerateGameModeListWidget(TSharedPtr<FAllowedGameModeData> InItem);

	void OnGameModeChanged(TSharedPtr<FAllowedGameModeData> NewSelection, ESelectInfo::Type SelectInfo);

	void ChangeGameModePanel();

	TSharedRef<SWidget> BuildGameModeWidget();

	FString GetGameModeText() const;
	FString GameModeDisplayName;

	// Builds the current GameOptions panel for the current game type.  
	void BuildGameOptionsPanel();

	FOnMatchInfoGameModeChanged OnMatchInfoGameModeChangedDelegate;
	
	// When the host changes the game mode and the data is replicated, non-hosts use this function to rebuild their settings/map panel.
	void OnMatchGameModeChanged();

	TSharedPtr<SWidget> Settings;
	
	TSharedRef<SWidget> BuildHostOptionWidgets();
	void JoinAnyTimeChanged(ECheckBoxState NewState);
	void AllowSpectatorChanged(ECheckBoxState NewState);
	void RankCeilingChanged(ECheckBoxState NewState);

	FText GetHostMaxPlayerLabel() const;
	FText GetMaxPlayerLabel() const;

	void OnMaxPlayersChanged(float NewValue);
};

#endif