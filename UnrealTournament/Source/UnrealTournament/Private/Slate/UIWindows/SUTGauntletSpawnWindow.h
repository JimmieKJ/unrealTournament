// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../FLoadoutData.h"
#include "../Base/SUTWindowBase.h"
#include "SUTSpawnWindow.h"

#if !UE_SERVER

class SUTLoadoutUpgradeWindow;

class UNREALTOURNAMENT_API SUTGauntletSpawnWindow : public SUTSpawnWindow
{
public:
	// Allows this window to be identified as a given type.
	virtual FName IdentifyWindow()
	{
		return FName(TEXT("GauntletSpawnWindow"));
	}

	virtual EInputMode::Type GetInputMode();

	void CloseUpgrade();

	virtual bool CanWindowClose();

protected:
	virtual void BuildWindow();
	void BuildSpawnMessage(float YPosition);
	void BuildLoadoutSlots(float YPosition);
	FText GetCurrentText() const;

	FText GetLoadoutText(int32 RoundMask) const;
	EVisibility UpgradesAvailable(int32 bSecondary) const;
	const FSlateBrush* GetLoadoutImage(bool bSecondary) const;
	FReply ItemClick(bool bSecondary);

	FText GetCurrencyText() const;

	TSharedPtr<SUTLoadoutUpgradeWindow> LoadoutUpgradeWindow;
};

#endif
