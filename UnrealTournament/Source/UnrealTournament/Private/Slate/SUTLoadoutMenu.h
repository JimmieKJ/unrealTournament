// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "FLoadoutData.h"
#include "SUWindowsDesktop.h"

#if !UE_SERVER

class SUTLoadoutMenu : public SUWindowsDesktop
{
protected:
	virtual void CreateDesktop();
	void CollectItems();

	TSharedPtr<FLoadoutData> CurrentItem;

	TArray<TSharedPtr<FLoadoutData>> AvailableItems;
	TArray<TSharedPtr<FLoadoutData>> SelectedItems;

	float TotalCostOfCurrentLoadout;
	void TallyLoadout();

	FText GetLoadoutTitle() const;

	const FSlateBrush* GetItemImage() const;
	FText GetItemDescriptionText() const;

	FReply OnAcceptClicked();
	bool OnAcceptEnabled() const;
	FReply OnCancelledClicked();

	TSharedPtr<SGridPanel> AvailableItemsPanel;
	TSharedPtr<SGridPanel> SelectedItemsPanel;

	void RefreshAvailableItemsList();
	void RefreshSelectedItemsList();

	void AvailableUpdateItemInfo(int32 Index);

	FReply OnAvailableClick(int32 Index);
	FReply OnSelectedClick(int32 Index);
	FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

};

#endif
