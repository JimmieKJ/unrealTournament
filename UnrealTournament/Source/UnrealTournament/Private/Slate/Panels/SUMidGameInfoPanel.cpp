

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "../Public/UTLocalPlayer.h"
#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "UTGameEngine.h"
#include "UTLobbyGameState.h"
#include "SUMidGameInfoPanel.h"


#if !UE_SERVER

struct FCompareUsersMidGame
{
	FORCEINLINE bool operator()	( const TSharedPtr< FSimpleListData > A, const TSharedPtr< FSimpleListData > B ) const 
	{
		if (A->DisplayColor != B->DisplayColor)
		{
			if (A->DisplayColor != FLinearColor::Gray && B->DisplayColor == FLinearColor::Gray) return true;
			if (A->DisplayColor == FLinearColor::Gray && B->DisplayColor != FLinearColor::Gray) return false;
			return (A->DisplayColor == FLinearColor::Red);
		}
		return ( A->DisplayText> B->DisplayText);
	}
};

void SUMidGameInfoPanel::SortUserList()
{
	UserList.Sort(FCompareUsersMidGame());
}


void SUMidGameInfoPanel::ConstructPanel(FVector2D ViewportSize)
{
	SUChatPanel::ConstructPanel(ViewportSize);
	Tag = FName(TEXT("MidGameInfoPanel"));
}

void SUMidGameInfoPanel::BuildNonChatPanel()
{
	if (NonChatPanel.IsValid())
	{
		NonChatPanel->ClearChildren();
		NonChatPanel->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Fill)
		[
			SNew(SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image(SUWindowsStyle::Get().GetBrush("UWindows.Standard.DarkBackground"))
			]
			+SOverlay::Slot()
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()		// Server Title
				[
					SAssignNew(ServerName,STextBlock)
					.Text(this, &SUMidGameInfoPanel::GetServerName)
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MOTD.ServerTitle")
				]

				+SVerticalBox::Slot()		// Server Rules
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(ServerRules,STextBlock)
					.Text(this, &SUMidGameInfoPanel::GetServerRules)
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MOTD.RulesText")
				]

				+SVerticalBox::Slot()		// Server MOTD
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				[
					SAssignNew(ServerMOTD,STextBlock)
					.Text(this, &SUMidGameInfoPanel::GetServerMOTD)
					.TextStyle(SUWindowsStyle::Get(), "UWindows.Standard.MOTD.GeneralText")
				]
			]
		];
	}

}

FString SUMidGameInfoPanel::GetServerName() const
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		return GameState->ServerName;
	}
	return TEXT("");
}

FText SUMidGameInfoPanel::GetServerMOTD() const
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		return FText::FromString(GameState->ServerMOTD);
	}
	return FText::FromString(TEXT(""));
}
 
FText SUMidGameInfoPanel::GetServerRules() const
{
	AUTGameState* GameState = PlayerOwner->GetWorld()->GetGameState<AUTGameState>();
	if (GameState)
	{
		return GameState->ServerRules();;
	}

	return FText::GetEmpty();
}

#endif