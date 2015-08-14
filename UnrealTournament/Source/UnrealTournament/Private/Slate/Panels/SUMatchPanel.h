// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "TAttributeProperty.h"
#include "UTLobbyMatchInfo.h"
#include "../Widgets/SUTComboButton.h"
#include "../Widgets/SUTPopOverAnchor.h"

#if !UE_SERVER

class FTrackedMatch : public TSharedFromThis<FTrackedMatch>
{
public:

	FGuid MatchId;
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;
	FString RuleTitle;
	FString MapName;
	int32 NumPlayers;
	int32 MaxPlayers;
	int32 NumFriends;
	uint32 Flags;
	int32 Rank;

	bool bPendingKill;

	FTrackedMatch()
	{
		MatchInfo.Reset();
	};

	FTrackedMatch(FGuid inMatchId, const FString& inRuleTitle, const FString& inMapName, const int32 inNumPlayers, const int32 inMaxPlayers, const int32 inNumFriends, const uint32 inFlags, const int32 inRank)
		: MatchId(inMatchId)
		, RuleTitle(inRuleTitle)
		, MapName(inMapName)
		, NumPlayers(inNumPlayers)
		, MaxPlayers(inMaxPlayers)
		, NumFriends(inNumFriends)
		, Flags(inFlags)
		, Rank(inRank)
	{
		MatchInfo.Reset();
		bPendingKill = false;
	}

	FTrackedMatch(const TWeakObjectPtr<AUTLobbyMatchInfo> inMatchInfo)
	{
		MatchInfo = inMatchInfo;
		bPendingKill = false;
	};

	static TSharedRef<FTrackedMatch> Make(FGuid inMatchId, const FString& inRuleTitle, const FString& inMapName, const int32 inNumPlayers, const int32 inMaxPlayers, const int32 inNumFriends, const uint32 inFlags, const int32 inRank)
	{
		return MakeShareable( new FTrackedMatch(inMatchId, inRuleTitle, inMapName, inNumPlayers, inMaxPlayers, inNumFriends, inFlags, inRank));
	}

	static TSharedRef<FTrackedMatch> Make(const TWeakObjectPtr<AUTLobbyMatchInfo> inMatchInfo)
	{
		return MakeShareable( new FTrackedMatch( inMatchInfo ) );
	}

	FText GetFlags()
	{
		if (MatchInfo.IsValid())
		{
			Flags = 0x00;
			// We don't have private yet.
			if (MatchInfo->bRankLocked)
			{
				Flags = Flags | 0x02;
			}
		}

		// TODO: Add icon references

		FString Final;
		if ((Flags & 0x01) == 0x01) Final = TEXT("<img src=\"UT.Icon.Lock.Small\"/> Passworded");
		if ((Flags & 0x02) == 0x02) Final = Final + (Final.IsEmpty() ? TEXT("") : TEXT("\n")) + TEXT("<img src=\"UT.Icon.Lock.Small\"/> Ranked");
		if ((Flags & 0x04) == 0x04) Final = Final + (Final.IsEmpty() ? TEXT("") : TEXT("\n")) + TEXT("<img src=\"UT.Icon.Lock.Small\"/> Private");


		if (NumFriends > 0)
		{
			FString Friends = FString::Printf(TEXT("<img src=\"UT.Icon.Friends.Small\"/> %i Friends Playing"), NumFriends);
			Final = Final + (Final.IsEmpty() ? TEXT("") : TEXT("\n")) + Friends;
		}
	
		return FText::FromString(Final);
	}


	FText GetCurrentPlayerCount()
	{
		return FText::AsNumber( (MatchInfo.IsValid() ? MatchInfo->NumPlayersInMatch() : NumPlayers) );
	}

	FText GetRuleTitle()
	{
		if ( MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid() )
		{
			return FText::FromString(MatchInfo->CurrentRuleset->Title);
		}

		return FText::FromString(RuleTitle);
	}

	FText GetMap()
	{
		if ( MatchInfo.IsValid() )
		{
			if (MatchInfo->InitialMapInfo.IsValid())
			{
				return FText::FromString(MatchInfo->InitialMapInfo->Title);
			}
			return FText::FromString(MatchInfo->InitialMap);
		}

		return FText::FromString(MapName);
	}

	FText GetMaxPlayers()
	{
		int32 MP = ( MatchInfo.IsValid() && MatchInfo->CurrentRuleset.IsValid() ) ? MatchInfo->CurrentRuleset->MaxPlayers : MaxPlayers;
		return FText::Format(NSLOCTEXT("SUMatchPanel","MaxPlayerFormat","Out of {0}"), FText::AsNumber(MP));
	}

	FSlateColor GetBadgeColor() const
	{
		int32 Badge;
		int32 Level;
		UUTLocalPlayer::GetBadgeFromELO((MatchInfo.IsValid() ? MatchInfo->AverageRank : Rank), Badge, Level);
		
		if (Badge == 1) return FSlateColor( FLinearColor(0.363f, 0.363f, 0.363f, 1.0f) );
		else if (Badge == 2) return FSlateColor( FLinearColor(0.496f, 0.27f, 0.10f, 1.0f) );

		return FSlateColor( FLinearColor(0.49f, 0.41f, 0.10f, 1.0f) );
	}


	FText GetRank()
	{
		int32 Badge;
		int32 Level;
		UUTLocalPlayer::GetBadgeFromELO((MatchInfo.IsValid() ? MatchInfo->AverageRank : Rank), Badge, Level);

		return FText::AsNumber(Level);
	}
};

class FServerData;

DECLARE_DELEGATE_TwoParams(FMatchPanelJoinMatchDelegate, const FString& , bool );

class UNREALTOURNAMENT_API SUMatchPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUMatchPanel)
	: _bExpectServerData(false)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_ARGUMENT( bool, bExpectServerData)
		SLATE_EVENT(FMatchPanelJoinMatchDelegate, OnJoinMatchDelegate )

	SLATE_END_ARGS()


public:	
	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	void SetServerData(TSharedPtr<FServerData> inServerData);

protected:
	TSharedPtr<FServerData> ServerData;
	bool bExpectServerData;

	/**
	 *	Returns INDEX_NONDE if not.
	 **/
	int32 IsTrackingMatch(AUTLobbyMatchInfo* Match);
	// Holds a list of matches that need to be displayed.

	TArray<TSharedPtr<FTrackedMatch> > TrackedMatches;
	TSharedRef<ITableRow> OnGenerateWidgetForMatchList( TSharedPtr<FTrackedMatch> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedPtr<SListView<TSharedPtr<FTrackedMatch>>> MatchList;
	TSharedPtr<SVerticalBox> NoMatchesBox;
	bool bShowingNoMatches;

	// The Player Owner that owns this panel
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;

	// Every frame check the status of the match and update.
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	FText GetServerNameText() const;
	FText GetMatchButtonText() const;
	
	FReply StartNewMatch();

	virtual void OnListMouseButtonDoubleClick(TSharedPtr<FTrackedMatch> SelectedMatch);

	virtual TSharedRef<SWidget> OnGetPopup(TSharedPtr<SUTPopOverAnchor> Anchor);
	virtual TSharedRef<SWidget> OnGetPopupContent(TSharedPtr<SUTPopOverAnchor> Anchor);

	FReply JoinMatchButtonClicked(TSharedPtr<FTrackedMatch> InItem);
	FReply SpectateMatchButtonClicked(TSharedPtr<FTrackedMatch> InItem);

	bool CanSpectateGame(TSharedPtr<FTrackedMatch> InItem) const;

	FMatchPanelJoinMatchDelegate OnJoinMatchDelegate;

};

#endif
