// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "FLoadoutData.h"
#include "SUWindowsDesktop.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTBuyMenu : public SUWindowsDesktop
{
protected:
	virtual void CreateDesktop();
	void CollectItems();

	TSharedPtr<FLoadoutData> CurrentItem;

	TArray<TSharedPtr<FLoadoutData>> AvailableItems;

	const FSlateBrush* GetItemImage() const;
	FText GetItemDescriptionText() const;

	FReply OnCancelledClicked();

	TSharedPtr<SGridPanel> AvailableItemsPanel;

	void RefreshAvailableItemsList();
	void AvailableUpdateItemInfo(int32 Index);
	FReply OnAvailableClick(int32 Index);
	FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);
};

#endif
