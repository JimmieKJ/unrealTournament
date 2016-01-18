// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUTProfileItemsDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTProfileItemsDialog)
		: _DialogSize(FVector2D(1000, 900))
		, _bDialogSizeIsRelative(false)
		, _DialogPosition(FVector2D(0.5f, 0.5f))
		, _DialogAnchorPoint(FVector2D(0.5f, 0.5f))
		, _ContentPadding(FVector2D(10.0f, 5.0f))
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)
	SLATE_ARGUMENT(FVector2D, DialogSize)
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)
	SLATE_ARGUMENT(FVector2D, DialogPosition)
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)
	SLATE_ARGUMENT(FVector2D, ContentPadding)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
protected:
	TArray< TSharedPtr<struct FProfileItemEntry> > Items;
	TSharedPtr< SListView< TSharedPtr<FProfileItemEntry> > > ItemList;

	TSharedRef<ITableRow> GenerateItemListRow(TSharedPtr<FProfileItemEntry> TheItem, const TSharedRef<STableViewBase>& OwningList);
};
#endif