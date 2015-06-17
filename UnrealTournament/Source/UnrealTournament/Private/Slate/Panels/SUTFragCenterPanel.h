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

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
protected:

	FString DesiredURL;
	bool bShowWarning;

	virtual bool BeforePopup(FString URL, FString Target);
	void WarningResult(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);
};

#endif