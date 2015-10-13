	// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SWidgetSwitcher.h"
#include "SUWDialog.h"
#include "Widgets/SUTTabButton.h"
#include "Widgets/SUTComboButton.h"
#include "UTRconAdminInfo.h"
#include "UTGameEngine.h"
#include "UTLobbyMatchInfo.h"
#include "Widgets/SUTPopOverAnchor.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SAdminPlayerRow : public SMultiColumnTableRow< TSharedPtr<FRconPlayerData> >
{
public:
	
	SLATE_BEGIN_ARGS( SAdminPlayerRow )
		: _PlayerData()
		{}

		SLATE_ARGUMENT( TSharedPtr<FRconPlayerData>, PlayerData)
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )


	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		PlayerData = InArgs._PlayerData;
		FSuperRowType::Construct( FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView );	
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FText ColumnText;
		if (PlayerData.IsValid())
		{
			if (ColumnName == FName(TEXT("PlayerName"))) 
			{
				ColumnText = FText::FromString(PlayerData->PlayerName);
			}
			else if (ColumnName == FName(TEXT("PlayerID"))) 
			{
				ColumnText = FText::FromString(PlayerData->PlayerID);
			}
			else if (ColumnName == FName(TEXT("PlayerIP"))) 
			{
				ColumnText = FText::FromString(PlayerData->PlayerIP);
			}
			else if (ColumnName == FName(TEXT("Rank"))) 
			{
				ColumnText = FText::AsNumber(PlayerData->AverageRank);
			}
			else if (ColumnName == FName(TEXT("InMatch"))) 
			{
				ColumnText = PlayerData->bInInstance ? NSLOCTEXT("Common","True","True") : NSLOCTEXT("Common","False","False");
			}
		}
		else
		{
			ColumnText = NSLOCTEXT("SUTAdminDialog","UnknownColumnText","n/a");
		}

		return SNew(STextBlock).Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal")).Text(ColumnText);
	}

	
private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FRconPlayerData> PlayerData;
};

struct FRconMatchData :  public TSharedFromThis<FRconMatchData>
{
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	bool bPendingDelete;
	FRconMatchData()
	{
		bPendingDelete = false;
	}

	FRconMatchData(AUTLobbyMatchInfo* inMatchInfo)
		: MatchInfo(inMatchInfo)
	{
		bPendingDelete = false;
	}

	static TSharedRef<FRconMatchData> Make(AUTLobbyMatchInfo* inMatchInfo)
	{
		return MakeShareable( new FRconMatchData(inMatchInfo));
	}

	FText GetMatchState()
	{
		if (MatchInfo.IsValid())
		{
			return FText::FromName(MatchInfo->CurrentState);
		}
		return FText::FromString(TEXT("N/A"));
	}

	FText GetOwnerName()
	{
		if (MatchInfo.IsValid())
		{
			return FText::FromString(MatchInfo->GetOwnerName());
		}
		return FText::FromString(TEXT("N/A"));
	}

	FText GetRuleset()
	{
		if (MatchInfo.IsValid())
		{
			return FText::FromString(MatchInfo->CurrentRuleset.IsValid() ? MatchInfo->CurrentRuleset->Title : TEXT("<none>"));
		}
		return FText::FromString(TEXT("N/A"));
	}

	FText GetMap()
	{
		if (MatchInfo.IsValid())
		{
			FString MapName = MatchInfo->InitialMapInfo.IsValid() ? MatchInfo->InitialMapInfo->Title : MatchInfo->InitialMap;
			return FText::FromString(MapName);
		}
		return FText::FromString(TEXT("N/A"));
	}

	FText GetNumPlayers()
	{
		if (MatchInfo.IsValid())
		{
			return FText::AsNumber(MatchInfo->NumPlayersInMatch() + MatchInfo->NumSpectatorsInMatch());
		}
		return FText::FromString(TEXT("N/A"));
	}

	FText GetJIP()
	{
		if (MatchInfo.IsValid())
		{
			return FText::FromString(MatchInfo->bJoinAnytime ? TEXT("Yes") : TEXT("No"));
		}
		return FText::FromString(TEXT("N/A"));
	
	}

	FText GetCanSpec()
	{
		if (MatchInfo.IsValid())
		{
			return FText::FromString(MatchInfo->bSpectatable? TEXT("Yes") : TEXT("No"));
		}
		return FText::FromString(TEXT("N/A"));
	
	}

	FText GetMatchFlags()
	{
		if (MatchInfo.IsValid())
		{
			return FText::AsNumber(MatchInfo->GetMatchFlags());
		}
		return FText::FromString(TEXT("N/A"));
	
	}



};


class UNREALTOURNAMENT_API SAdminMatchRow : public SMultiColumnTableRow< TSharedPtr<FRconMatchData> >
{
public:
	
	SLATE_BEGIN_ARGS( SAdminMatchRow )
		: _MatchData()
		{}

		SLATE_ARGUMENT( TSharedPtr<FRconMatchData>, MatchData)
		SLATE_STYLE_ARGUMENT( FTableRowStyle, Style )


	SLATE_END_ARGS()
	
	/**
	 * Construct the widget
	 *
	 * @param InArgs   A declaration from which to construct the widget
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		MatchData = InArgs._MatchData;
		FSuperRowType::Construct( FSuperRowType::FArguments().Padding(1).Style(InArgs._Style), InOwnerTableView );	
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		FText ColumnText;

		if (MatchData.IsValid() && MatchData->MatchInfo.IsValid())
		{
			if (ColumnName == FName(TEXT("MatchState")))
			{
				return SNew(STextBlock)
				.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(MatchData.Get(), &FRconMatchData::GetMatchState)));
			}

			if (ColumnName == FName(TEXT("OwnerName"))) 
			{
				return SNew(STextBlock)
				.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(MatchData.Get(), &FRconMatchData::GetOwnerName)));
			}
			else if (ColumnName == FName(TEXT("Ruleset"))) 
			{
				return SNew(STextBlock)
				.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(MatchData.Get(), &FRconMatchData::GetRuleset)));
			}
			else if (ColumnName == FName(TEXT("Map"))) 
			{
				return SNew(STextBlock)
				.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(MatchData.Get(), &FRconMatchData::GetMap)));
			}
			else if (ColumnName == FName(TEXT("NumPlayers"))) 
			{
				return SNew(STextBlock)
				.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(MatchData.Get(), &FRconMatchData::GetNumPlayers)));
			}
			else if (ColumnName == FName(TEXT("JIP"))) 
			{
				return SNew(STextBlock)
				.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(MatchData.Get(), &FRconMatchData::GetJIP)));
			}
			else if (ColumnName == FName(TEXT("CanSpec"))) 
			{
				return SNew(STextBlock)
				.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(MatchData.Get(), &FRconMatchData::GetCanSpec)));
			}
			else if (ColumnName == FName(TEXT("Flags"))) 
			{
				return SNew(STextBlock)
				.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
				.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(MatchData.Get(), &FRconMatchData::GetMatchFlags)));
			}
		}

		return SNew(STextBlock)
			.Font(SUTStyle::Get().GetFontStyle("UT.Font.ServerBrowser.List.Normal"))
			.Text(NSLOCTEXT("SUTAdminDialog","UnknownColumnText","n/a"));
		
	}

private:

	/** A pointer to the data item that we visualize/edit */
	TSharedPtr<FRconMatchData> MatchData;
};




class UNREALTOURNAMENT_API SUTAdminDialog : public SUWDialog
{
	SLATE_BEGIN_ARGS(SUTAdminDialog)
	: _DialogTitle(NSLOCTEXT("SUTAdminDialog", "Title", "ADMIN PANEL"))
	, _DialogSize(FVector2D(1800, 1000))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_ARGUMENT(TWeakObjectPtr<class AUTRconAdminInfo>, AdminInfo)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual void OnDialogClosed() override;

protected:

	TWeakObjectPtr<AUTRconAdminInfo> AdminInfo;
	TSharedPtr<SWidgetSwitcher> Switcher;

	TSharedPtr<SHorizontalBox> MatchInfoBox;

	TArray<TSharedPtr<FRconPlayerData>> SortedPlayerList;
	TArray<TSharedPtr<FRconMatchData>> SortedMatchList;

	TSharedPtr<SListView<TSharedPtr<FRconPlayerData>>> PlayerList;
	TSharedPtr<SListView<TSharedPtr<FRconMatchData>>> MatchList;
	TArray<TSharedPtr<SUTButton>> ButtonList;

	FReply ChangeTab(int32 WidgetTag);

	void AddPlayerPanel(TSharedPtr<SWidgetSwitcher> Switcher, TSharedPtr<SHorizontalBox> ButtonBox);
	void AddMatchPanel(TSharedPtr<SWidgetSwitcher> Switcher, TSharedPtr<SHorizontalBox> ButtonBox);

	TSharedRef<ITableRow> OnGenerateWidgetForPlayerList( TSharedPtr<FRconPlayerData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedRef<ITableRow> OnGenerateWidgetForMatchList( TSharedPtr<FRconMatchData> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	TSharedPtr<SEditableTextBox> UserMessage;
	FReply MessageUserClicked();
	FReply MessageAllUsersClicked();
	FReply KickBanClicked(bool bBan);

	void OnMatchListSelectionChanged(TSharedPtr<FRconMatchData> SelectedItem, ESelectInfo::Type SelectInfo);
	virtual void UpdateMatchInfo();
	virtual FReply KillMatch();
};

#endif