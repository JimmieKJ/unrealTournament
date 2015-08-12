// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUTStyle.h"
#include "TAttributeProperty.h"
#include "UTLobbyMatchInfo.h"
#include "../Widgets/SUTMenuAnchor.h"

#if !UE_SERVER

namespace ETrackedPlayerType
{
	enum Type
	{
		Player,
		MatchHeader,
		EveryoneHeader, 
		InstancePlayer,
		MAX,
	};
};


class FTrackedPlayer : public TSharedFromThis<FTrackedPlayer>
{
public:
	// What type of cell is this.
	ETrackedPlayerType::Type EntryType;

	// The Unique Id of this player
	FUniqueNetIdRepl PlayerID;

	// This players Name
	FString PlayerName;

	// The ID of this player's Avatar
	int32 AvatarID;

	// Will be true if this player is in the same match as the owner
	bool bIsInMatch;

	// Will be true if this player is the host of a match (only useful in hubs)
	bool bIsHost;

	// Will be true if this player is the owner
	bool bIsOwner;

	// Runtime.. each frame this will be set to pending kill and cleared if we are still relevant
	bool bPendingKill;

	// Will be true if this player is in an instance
	bool bInInstance;

	// This is the last known team num.  This will be used to resort the array when a player's team has changed. 
	uint8 TeamNum;

	float SortOrder;

	TWeakObjectPtr<AUTPlayerState> PlayerState;

	FTrackedPlayer(FString inHeaderText, ETrackedPlayerType::Type inEntryType)
		: EntryType(inEntryType)
		, PlayerName(inHeaderText)
	{
		bPendingKill = false;
		TeamNum = 255;
		bIsInMatch = false;
		bIsHost = false;
		bInInstance = false;
	}

	FTrackedPlayer(TWeakObjectPtr<AUTPlayerState> inPlayerState, FUniqueNetIdRepl inPlayerID, const FString& inPlayerName, uint8 inTeamNum, int32 inAvatarID, bool inbIsOwner, bool inbIsHost)
		: PlayerID(inPlayerID)
		, PlayerName(inPlayerName)
		, AvatarID(inAvatarID)
		, bIsHost(inbIsHost)
		, bIsOwner(inbIsOwner)
		, TeamNum(inTeamNum)
		, PlayerState(inPlayerState)
	{
		bPendingKill = false;
		TeamNum = 255;
		bIsInMatch = false;
		bIsHost = false;
		EntryType = ETrackedPlayerType::Player;
		bInInstance = inPlayerState == NULL;
	}

	static TSharedRef<FTrackedPlayer> Make(TWeakObjectPtr<AUTPlayerState> inPlayerState, FUniqueNetIdRepl inPlayerID, const FString& inPlayerName, uint8 inTeamNum, int32 inAvatarID, bool inbIsOwner, bool inbIsHost)
	{
		return MakeShareable( new FTrackedPlayer(inPlayerState, inPlayerID, inPlayerName, inTeamNum, inAvatarID, inbIsOwner, inbIsHost));
	}

	static TSharedRef<FTrackedPlayer> MakeHeader(FString inHeaderText, ETrackedPlayerType::Type inEntryType)
	{
		return MakeShareable( new FTrackedPlayer(inHeaderText, inEntryType));
	}

	FText GetPlayerName()
	{
		return PlayerState.IsValid() ? FText::FromString(PlayerState->PlayerName) : FText::FromString(PlayerName);
	}

	FSlateColor GetNameColor() const
	{
		if (bIsInMatch)
		{
			if (TeamNum == 0) return FSlateColor(FLinearColor(1.0f, 0.05f, 0.0f, 1.0f));
			else if (TeamNum == 1) return FSlateColor(FLinearColor(0.1f, 0.1f, 1.0f, 1.0f));
		}

		return FSlateColor(FLinearColor::Gray);
	}

	FText GetLobbyStatusText()
	{
		if (bIsInMatch && PlayerState.IsValid())
		{
			if (bIsHost) 
			{
				return NSLOCTEXT("Generic","Host","HOST");
			}

			return PlayerState->bReadyToPlay ? NSLOCTEXT("Generic","Ready","READY") : NSLOCTEXT("Generic","NotReady","NOT READY");
		}

		return FText::GetEmpty();
	}

	const FSlateBrush* GetAvatar() const
	{
		FName AvatarName = FName("UT.Avatar.0");
		return SUTStyle::Get().GetBrush(AvatarName);			
	}
};

DECLARE_DELEGATE_OneParam(FPlayerClicked, FUniqueNetIdRepl);

class SUTextChatPanel;

class UNREALTOURNAMENT_API SUPlayerListPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUPlayerListPanel)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_EVENT(FPlayerClicked, OnPlayerClicked)
	SLATE_END_ARGS()


public:	
	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	void ForceRefresh();

	TSharedPtr<SUTextChatPanel> ConnectedChatPanel;

protected:

	void CheckFlags(bool &bIsHost, bool &bIsTeamGame);

	TSharedPtr<FTrackedPlayer> MatchHeader;
	TSharedPtr<FTrackedPlayer> EveryoneHeader;
	TSharedPtr<FTrackedPlayer> InstanceHeader;

	bool bNeedsRefresh;

	// The Player Owner that owns this panel
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;


	TArray<TSharedPtr<FTrackedPlayer>> TrackedPlayers;
	TSharedRef<ITableRow> OnGenerateWidgetForPlayerList( TSharedPtr<FTrackedPlayer> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedPtr<SListView<TSharedPtr<FTrackedPlayer>>> PlayerList;

	// Checks to see if a player id is being tracked.  Returns the index into the TrackedPlayer's array or INDEX_NONE
	int32 IsTracked(const FUniqueNetIdRepl& PlayerID);

	// Every frame check the status of the match and update.
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	TSharedRef<SWidget> GetPlayerPortrait(TSharedPtr<FTrackedPlayer> Player);

	// Returns true if a given player is in the owner's match
	bool IsInMatch(AUTPlayerState* PlayerState);

	// Returns true if this player should be shown in the list.

	bool ShouldShowPlayer(FUniqueNetIdRepl PlayerId, uint8 TeamNum, bool bIsInMatch);

	FPlayerClicked PlayerClickedDelegate;

	FReply OnListSelect(TSharedPtr<FTrackedPlayer> Selected);

	void GetMenuContent(FString SearchTag, TArray<FMenuOptionData>& MenuOptions);

	void OnSubMenuSelect(FName Tag, TSharedPtr<FTrackedPlayer> InItem);
};

#endif
