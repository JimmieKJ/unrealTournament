// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTOnlineGameSettingsBase.h"
#include "SlateBasics.h"
#include "../Base/SUTWindowBase.h"

#if !UE_SERVER

const int32 PING_ALLOWANCE = 30;
const int32 PLAYER_ALLOWANCE = 5;

class SUTLobbyInfoPanel;

class UNREALTOURNAMENT_API SUTStartMatchWindow: public SUTWindowBase
{
public:

	SLATE_BEGIN_ARGS(SUTStartMatchWindow)
	{}

	SLATE_ARGUMENT(bool, bIsHost)
	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner);
	virtual void BuildWindow();

	virtual void TellSlateIWantKeyboardFocus();

	TSharedPtr<SUTLobbyInfoPanel> ParentPanel;

protected:

	bool bIsHost;
	float ElapsedTime;

	/** Holds a reference to the SCanvas widget that makes up the dialog */
	TSharedPtr<SCanvas> Canvas;

	/** Holds a reference to the SOverlay that defines the content for this dialog */
	TSharedPtr<SOverlay> WindowContent;

	FText GetMainText() const;
	FText GetMinorText() const;
	FText GetStatusText() const;
private:

	bool bAwaitingMatchInfo;

	virtual bool SupportsKeyboardFocus() const override;

	FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
	FReply OnCancelClick();

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	TArray<FString> Funny;
	float FunnyTimer;
	int32 FunnyIndex;
};

#endif