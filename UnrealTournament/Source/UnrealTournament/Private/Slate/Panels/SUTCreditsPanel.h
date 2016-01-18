// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "SUWindowsStyle.h"
#include "../Base/SUTPanelBase.h"
#include "SUTWebBrowserPanel.h"
#include "UTLocalPlayer.h"

#if !UE_SERVER

#include "SWebBrowser.h"

class UNREALTOURNAMENT_API SUTCreditsPanel : public SUTPanelBase
{
	virtual void ConstructPanel(FVector2D ViewportSize);

	virtual void OnShowPanel(TSharedPtr<SUTMenuBase> inParentWindow) override;

protected:
	
	TSharedPtr<SHorizontalBox> WebBrowserBox;
	TSharedPtr<SUTWebBrowserPanel> CreditsWebBrowser;
};

#endif