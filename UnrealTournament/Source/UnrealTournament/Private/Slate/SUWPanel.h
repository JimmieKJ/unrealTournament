// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SlateBasics.h"
#include "SUWindowsDesktop.h"

#if !UE_SERVER

class SUWPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SUWPanel)
	{}

	SLATE_ARGUMENT(TWeakObjectPtr<UUTLocalPlayer>, PlayerOwner)

	SLATE_END_ARGS()

public:
	/** needed for every widget */
	void Construct(const FArguments& InArgs);

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


	void ConsoleCommand(FString Command);


protected:

	// A TAG that can quickly describe this panel
	FName Tag;

	// The Player Owner that owns this panel
	TWeakObjectPtr<UUTLocalPlayer> PlayerOwner;

	// The Window that contains this panel.  NOTE: this will only be valid if this panel is contained within an SUWindowsDesktop.
	TSharedPtr<SUWindowsDesktop> ParentWindow;

};

#endif