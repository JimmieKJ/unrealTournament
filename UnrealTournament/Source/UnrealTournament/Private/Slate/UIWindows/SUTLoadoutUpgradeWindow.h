// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "../FLoadoutData.h"
#include "../Base/SUTWindowBase.h"
#include "UTReplicatedLoadoutInfo.h"

#if !UE_SERVER

class SUTGauntletSpawnWindow;

class UNREALTOURNAMENT_API SUTLoadoutUpgradeWindow : public SUTWindowBase
{
public:
	void Construct(const FArguments& InArgs, TWeakObjectPtr<UUTLocalPlayer> InPlayerOwner, bool bShowSecondayItems);
	virtual void BuildWindow();

	TSharedPtr<SUTGauntletSpawnWindow> SpawnWindow;

protected:
	bool bSecondaryItems;
	void CollectItems();

	TWeakObjectPtr<AUTReplicatedLoadoutInfo> CurrentItem;

	TArray<TWeakObjectPtr<AUTReplicatedLoadoutInfo>> AvailableItems;
	TSharedPtr<SGridPanel> ItemsPanel;

	void RefreshAvailableItemsList();
	void AvailableUpdateItemInfo(int32 Index);
	FReply OnAvailableClick(int32 Index);

	FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

};


#endif
