// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SUWindowsDesktop.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUWPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWPanel)
	{}
	
	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> PlayerOwner);

	virtual void ConstructPanel(FVector2D CurrentViewportSize);
	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow);
	virtual void OnHidePanel();

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}

	inline TSharedPtr<SUWindowsDesktop> GetParentWindow()
	{
		return ParentWindow;
	}

	// I can't believe how slate manages slots.
	int32 ZOrder;

	AUTPlayerState* GetOwnerPlayerState();

	void ConsoleCommand(FString Command);

	// Used by generic lists to generate string widgets for each item
	TSharedRef<SWidget> GenerateStringListWidget(TSharedPtr<FString> InItem);

	// Return true to show the back button in the conatining desktop
	virtual bool ShouldShowBackButton()
	{
		return true;
	}

protected:

	// Will be true if this panel is closing
	bool bClosing;

	// A TAG that can quickly describe this panel
	FName Tag;

	// The Player Owner that owns this panel
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;

	// The Window that contains this panel.  NOTE: this will only be valid if this panel is contained within an SUWindowsDesktop.
	TSharedPtr<SUWindowsDesktop> ParentWindow;


};

#endif