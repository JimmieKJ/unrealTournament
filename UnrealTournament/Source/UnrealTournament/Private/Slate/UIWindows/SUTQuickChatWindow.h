// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../Base/SUTWindowBase.h"


#if !UE_SERVER
class SUTChatBar;

class UNREALTOURNAMENT_API SUTQuickChatWindow: public SUTWindowBase
{
	SLATE_BEGIN_ARGS(SUTQuickChatWindow)
	{}

	SLATE_ARGUMENT(FName, InitialChatDestination)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner);
	virtual void BuildWindow();

	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InKeyboardFocusEvent ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyboardEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;


protected:
	FName ChatDestination;
	TSharedPtr<SUTChatBar> ChatBar;
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	void ChatTextCommited(const FText& NewText, ETextCommit::Type CommitType);

private:

};
#endif
