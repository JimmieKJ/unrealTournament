// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTStartMatchWindow.h"
#include "../SUTStyle.h"
#include "../Widgets/SUTButton.h"
#include "UTLobbyPlayerState.h"
#include "../Panels/SUTLobbyInfoPanel.h"

#if !UE_SERVER

void SUTStartMatchWindow::Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner)
{
	PlayerOwner = InPlayerOwner;
	bIsHost = InArgs._bIsHost;

	Funny.Add(TEXT("Constructioning navigation mesh..."));
	Funny.Add(TEXT("Teaching A.I. to walk..."));
	Funny.Add(TEXT("Adjusting lights..."));
	Funny.Add(TEXT("Gib clean-up in zone 3..."));
	Funny.Add(TEXT("Turning on fog machine..."));
	Funny.Add(TEXT("Performing some dark magic..."));
	Funny.Add(TEXT("Spell checking..."));
	Funny.Add(TEXT("Activating the Field LAttice Generator..."));
	Funny.Add(TEXT("Taking a WOW break..."));
	Funny.Add(TEXT("Dividing by Zero..."));
	Funny.Add(TEXT("Tighening up the graphics..."));
	Funny.Add(TEXT("Lets BOUNCE!!!"));
	FunnyIndex = -1;
	SUTWindowBase::Construct
	(
		SUTWindowBase::FArguments()
			.Size(FVector2D(800, 220))
			.bSizeIsRelative(false)
			.Position(FVector2D(0.5f, 0.5f))
			.AnchorPoint(FVector2D(0.5f, 0.5f))
			.bShadow(true)
		, PlayerOwner

	);
	ElapsedTime	= 0.0f;
	bAwaitingMatchInfo = true;
}

void SUTStartMatchWindow::BuildWindow()
{
	// this is the background image
	Content->AddSlot()
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill)
		[
			SNew(SImage)
			.Image(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
		]
	];

	// This will define a vertical box that holds the various components of the dialog box.
	Content->AddSlot()
	[
		SNew(SVerticalBox)

		// The title bar
		+ SVerticalBox::Slot()						
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		.FillHeight(1.0f)
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SUTStartMatchWindow::GetMainText)
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
		]
		+ SVerticalBox::Slot()						
		.Padding(0.0f, 5.0f, 0.0f, 5.0f)
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &SUTStartMatchWindow::GetMinorText)
			.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
		]
		+ SVerticalBox::Slot()
		.Padding(12.0f, 25.0f, 12.0f, 25.0f)
		.AutoHeight()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SUTStartMatchWindow::GetStatusText)
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium")
			]
			+SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoHeight()
			.Padding(0.0f,10.0f,0.0f,0.0f)
			[
				SNew(SUTButton)
				.ButtonStyle(SUTStyle::Get(), "UT.SimpleButton.Dark")
				.OnClicked(this, &SUTStartMatchWindow::OnCancelClick)
				.ContentPadding(FMargin(25.0, 0.0, 25.0, 5.0))
				.Text(NSLOCTEXT("QuickMatch", "CancelText", "ESC to Cancel"))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Small")
			]
		]
	];

	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}

FText SUTStartMatchWindow::GetMainText() const
{
	AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
	if (LobbyPlayerState)
	{
		if (bIsHost)
		{
			if (LobbyPlayerState->CurrentMatch == nullptr)
			{
				return NSLOCTEXT("SUTStartMatchWindow","CreatingMatch","Server is starting a match...");
			}
		}
		return NSLOCTEXT("SUTStartMatchWindow","ConfigMatch","Configuring Game...");
	}
	return NSLOCTEXT("SUTStartMatchWindow","BadMainText","Broken - Please Fix");
}

FText SUTStartMatchWindow::GetMinorText() const
{
	AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
	if (LobbyPlayerState)
	{
		if (bIsHost)
		{
			if (LobbyPlayerState->CurrentMatch == nullptr)
			{
				return NSLOCTEXT("SUTStartMatchWindow","CreatingMatchMinor","Awaiting Initial Replication");
			}
		}

		if (LobbyPlayerState->CurrentMatch != nullptr && LobbyPlayerState->CurrentMatch->CurrentRuleset.IsValid())
		{
			if (FunnyIndex < 0 || !Funny.IsValidIndex(FunnyIndex))
			{
				FString MapName = LobbyPlayerState->CurrentMatch->InitialMapInfo.IsValid() ? LobbyPlayerState->CurrentMatch->InitialMapInfo->Title : LobbyPlayerState->CurrentMatch->InitialMap;
				return FText::Format(NSLOCTEXT("SUTStartMatchWindow","ConfigMatchMinorFormat","Settings up {0} on {1}"), FText::FromString(LobbyPlayerState->CurrentMatch->CurrentRuleset->Title), FText::FromString(MapName) );
			}
			else
			{
				return FText::FromString(Funny[FunnyIndex]);
			}
		}
	}
	return FText::GetEmpty();

}

FText SUTStartMatchWindow::GetStatusText() const
{
	return FText::Format(NSLOCTEXT("QuickMatch","StatusFormatStr","Elapsed time... ({0} seconds)"), FText::AsNumber(int32(ElapsedTime)));
}


bool SUTStartMatchWindow::SupportsKeyboardFocus() const
{
	return true;
}


FReply SUTStartMatchWindow::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		return OnCancelClick();
	}

	return FReply::Unhandled();
}


FReply SUTStartMatchWindow::OnCancelClick()
{
	if (ParentPanel.IsValid())
	{
		ParentPanel->CancelInstance();
	}
	
	return FReply::Handled();
}

void SUTStartMatchWindow::TellSlateIWantKeyboardFocus()
{
	FSlateApplication::Get().SetKeyboardFocus(SharedThis(this), EKeyboardFocusCause::Keyboard);
}

void SUTStartMatchWindow::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	ElapsedTime += InDeltaTime;

	AUTLobbyPlayerState* LobbyPlayerState = Cast<AUTLobbyPlayerState>(PlayerOwner->PlayerController->PlayerState);
	if (LobbyPlayerState )
	{
		if (bAwaitingMatchInfo && LobbyPlayerState->CurrentMatch != nullptr)
		{
			bAwaitingMatchInfo = false;
			ParentPanel->ApplySetup(LobbyPlayerState->CurrentMatch);
		}

		if (LobbyPlayerState->CurrentMatch != nullptr && LobbyPlayerState->CurrentMatch->CurrentRuleset.IsValid())
		{
			FunnyTimer += InDeltaTime;
			if (FunnyTimer < 1.5f || Funny.Num() == 0)
			{
				FunnyIndex = -1;
			}
			else
			{
				FunnyIndex = FMath::Clamp<int32>(int32( (FunnyTimer - 1.5f) / 1.5f ), 0, Funny.Num()-1);
			}
		}
	}

}


#endif