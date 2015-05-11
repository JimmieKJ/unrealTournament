// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#if !UE_SERVER

//class declare
class UNREALTOURNAMENT_API SUTFriendsWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUTFriendsWidget)
		: _OnClose()
	{}

		/** Called when the button is clicked */
		SLATE_EVENT(FOnClicked, OnClose)

	/** Always at the end */
	SLATE_END_ARGS()

	/** Destructor */
	virtual ~SUTFriendsWidget();

	/** Needed for every widget */
	void Construct(const FArguments& InArgs, const FLocalPlayerContext& InCtx);

private:

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Called when the widget wishes to close */
	FReply OnClose();

	/** Called to close the widget */
	FOnClicked OnCloseDelegate;

	/** Holder of the content */
	TSharedPtr<SWeakWidget> ContentWidget;

	/** Player context */
	FLocalPlayerContext Ctx;
};

#endif // !UE_SERVER
