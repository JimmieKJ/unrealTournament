// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"

#if !UE_SERVER

#include "SWebBrowser.h"

class UNREALTOURNAMENT_API SUTWebBrowserPanel : public SUWPanel
{
	SLATE_BEGIN_ARGS(SUTWebBrowserPanel)
	: _ShowControls(true)
	{}

	SLATE_ARGUMENT(bool, ShowControls)

	SLATE_END_ARGS()
	
public:

	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner);

	virtual void ConstructPanel(FVector2D ViewportSize);
	virtual void Browse(FString URL);

protected:

	TSharedPtr<SVerticalBox> WebBrowserContainer;
	// The Actual Web browser panel.
	TSharedPtr<SWebBrowser> WebBrowserPanel;

	float GetReverseScale() const;
	bool ShowControls;
};

#endif