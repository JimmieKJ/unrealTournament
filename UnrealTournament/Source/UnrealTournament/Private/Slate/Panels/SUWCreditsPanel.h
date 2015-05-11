// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWindowsStyle.h"
#include "../SUWPanel.h"
#include "UTLocalPlayer.h"

#if !UE_SERVER

#include "SWebBrowser.h"

class UNREALTOURNAMENT_API SUWCreditsPanel : public SUWPanel
{
	virtual void ConstructPanel(FVector2D ViewportSize);

	virtual void OnShowPanel(TSharedPtr<SUWindowsDesktop> inParentWindow) override;

protected:
	
	TSharedPtr<SHorizontalBox> WebBrowserBox;
	TSharedPtr<SWebBrowser> CreditsWebBrowser;
};

#endif