// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"

class SUWDialog : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWDialog)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)
	SLATE_END_ARGS()

	/** needed for every widget */
	void Construct(const FArguments& InArgs);

	virtual void OnDialogOpened();
	virtual void OnDialogClosed();

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}
public:
	/** utility to generate a simple text widget for list and combo boxes given a string value */
	TSharedRef<SWidget> GenerateStringListWidget(TSharedPtr<FString> InItem);
private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
	TSharedPtr<class SWidget> GameViewportWidget;

	// HACKS needed to keep window focus
	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyboardFocusReceived(const FGeometry& MyGeometry, const FKeyboardFocusEvent& InKeyboardFocusEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyboardEvent& InKeyboardEvent) override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;


};