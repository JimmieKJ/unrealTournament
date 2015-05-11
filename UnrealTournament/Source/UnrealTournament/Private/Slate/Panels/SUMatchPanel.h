// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "TAttributeProperty.h"
#include "UTLobbyMatchInfo.h"
#include "../Widgets/SUTComboButton.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUMatchPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUMatchPanel)
		: _MatchInfo(NULL)
		, _bIsFeatured(false)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<AUTLobbyMatchInfo>, MatchInfo )
		SLATE_ARGUMENT( TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner )
		SLATE_ARGUMENT( bool, bIsFeatured )

		/** Called when the Play/Jo */
		SLATE_EVENT( FOnClicked, OnClicked )

	SLATE_END_ARGS()


public:	
	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	
protected:

	// The Player Owner that owns this panel
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;

	// Used to store all of the attributes to be displayed.
	TArray<TSharedPtr<TAttributePropertyBase>> DataStore;
	
	// Holds the match info associated with this match...
	UPROPERTY()
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	// Name of the host of this panel
	FString HostName;

	// The GameMode which this match panel represents.  If it changes, it will destroy the surface of the button and start over.
	FString CurrentGameModeClass;

	// holds the current match state.  If it changes, then destroy the surface and recreate.
	FName CurrentMatchState;

	TSharedPtr<SVerticalBox> ButtonSurface;

	FText GetButtonText() const;

	bool bIsFeatured;
	
	FOnClicked OnClicked;

	// Every frame check the status of the match and update.
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	// Update/Create the panel
	void UpdateMatchInfo();

	FReply ButtonClicked();

	FText GetMatchBadgeText() const;

	const FSlateBrush* GetELOBadgeImage() const;
	const FSlateBrush* GetELOBadgeNumberImage() const;
	const FSlateBrush* GetMatchSlateBadge() const;

	FText GetTimeRemaining() const;
	FText GetRuleName() const;

	void OnSubMenuSelect(int32 MenuCmdId, TSharedPtr<SUTComboButton> Sender);

};

#endif
