// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"

#if !UE_SERVER

class FServerSearchInfo
{
public:
	FOnlineSessionSearchResult SearchResult;
	int32 Ping;
	int32 NoPlayers;

	FServerSearchInfo(const FOnlineSessionSearchResult& inSearchResult, int32 inPing, int32 inNoPlayers)
		: SearchResult(inSearchResult)
		, Ping(inPing)
		, NoPlayers(inNoPlayers)
	{
	}

	static TSharedRef<FServerSearchInfo> Make(const FOnlineSessionSearchResult& inSearchResult, int32 inPing, int32 inNoPlayers)
	{
		return MakeShareable(new FServerSearchInfo(inSearchResult, inPing, inNoPlayers));
	}

};

struct FServerSearchPingTracker
{
	TSharedPtr<FServerSearchInfo> Server;
	TWeakObjectPtr<AUTServerBeaconClient> Beacon;

	FServerSearchPingTracker(TSharedPtr<FServerSearchInfo> inServer, AUTServerBeaconClient* inBeacon)
		: Server(inServer)
		, Beacon(inBeacon)
	{
	};
};


class SUTQuickMatch: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SUTQuickMatch)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FName, QuickMatchType)
	SLATE_END_ARGS()


	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}

	// Called to cancel the quickmatch process
	void Cancel();

	virtual void TellSlateIWantKeyboardFocus();

protected:

	/** Holds a reference to the SCanvas widget that makes up the dialog */
	TSharedPtr<SCanvas> Canvas;

	/** Holds a reference to the SOverlay that defines the content for this dialog */
	TSharedPtr<SOverlay> WindowContent;


private:
	FText MinorStatusText;
	bool bSearchInProgress;

	IOnlineSubsystem* OnlineSubsystem;
	IOnlineIdentityPtr OnlineIdentityInterface;
	IOnlineSessionPtr OnlineSessionInterface;

	FDelegateHandle OnFindSessionCompleteHandle;

	TSharedPtr<class FUTOnlineGameSearchBase> SearchSettings;
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	FName QuickMatchType;
	float StartTime;

	FText GetStatusText() const;
	FText GetMinorStatusText() const;

	// Begin the search for a HUB to join
	void FindHUBToJoin();

	virtual void OnServerBeaconResult(AUTServerBeaconClient* Sender, FServerBeaconInfo ServerInfo);
	virtual void OnServerBeaconFailure(AUTServerBeaconClient* Sender);
	void PingNextBatch();
	virtual void PingServer(TSharedPtr<FServerSearchInfo> ServerToPing);

	void OnFindSessionsComplete(bool bWasSuccessful);

	TArray<TSharedPtr<FServerSearchInfo>> ServerList;
	TArray<TSharedPtr<FServerSearchInfo>> FinalList;
	TArray<FServerSearchPingTracker> PingTrackers;

	void NoAvailableMatches();
	void FindBestMatch();


	virtual bool SupportsKeyboardFocus() const override;

	FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	FReply OnCancelClick();
};

#endif