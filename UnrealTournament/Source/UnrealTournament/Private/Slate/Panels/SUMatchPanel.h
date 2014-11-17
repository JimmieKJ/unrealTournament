// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "UTLobbyMatchInfo.h"

#if !UE_SERVER

class SUMatchPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUMatchPanel)
		: _MatchInfo(NULL)
		, _bIsFeatured(false)
	{}
		SLATE_ARGUMENT( TWeakObjectPtr<AUTLobbyMatchInfo>, MatchInfo )
		SLATE_ARGUMENT( bool, bIsFeatured )

		/** Called when the Play/Jo */
		SLATE_EVENT( FOnClicked, OnClicked )

	SLATE_END_ARGS()


public:	
	/** needed for every widget */
	void Construct(const FArguments& InArgs);
	
protected:
	// Holds the match info associated with this match...

	UPROPERTY()
	TWeakObjectPtr<AUTLobbyMatchInfo> MatchInfo;

	FText GetButtonText() const;

	bool bIsFeatured;
	
	FOnClicked OnClicked;
	TSharedPtr<STextBlock> ActionText;
	TSharedPtr<STextBlock> MatchTitle;

	// Every frame check the status of the match and update.
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual void UpdateMatchInfo();
	FReply ButtonClicked();
};

#endif