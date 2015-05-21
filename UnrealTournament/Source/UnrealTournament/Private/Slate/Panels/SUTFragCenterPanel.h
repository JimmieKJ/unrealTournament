// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate/SlateGameResources.h"
#include "../SUWPanel.h"
#include "../SUWindowsStyle.h"
#include "SUTWebBrowserPanel.h"
#include "../Widgets/SUTButton.h"

#if !UE_SERVER

#include "SWebBrowser.h"

class UNREALTOURNAMENT_API SUTFragCenterPanel : public SUTWebBrowserPanel
{
public:
	virtual void ConstructPanel(FVector2D ViewportSize);

	void UpdateAutoPlay();

protected:
	TSharedPtr<SUTButton> AutoPlayToggleButton;
	FText GetAutoPlayText();
	FReply ChangeAutoPlay();

	virtual bool OnBrowse(FString TargetURL, bool bRedirect);

};

#endif