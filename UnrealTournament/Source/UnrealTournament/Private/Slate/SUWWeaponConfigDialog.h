// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"

#if !UE_SERVER
class SUWWeaponConfigDialog : public SUWDialog, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SUWWeaponConfigDialog)
		: _DialogSize(FVector2D(1000,900))
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

	TSharedPtr<SCheckBox> AutoWeaponSwitch;
	TArray<UClass*> WeaponList;
	TSharedPtr< SListView<UClass*> > WeaponPriorities;

	TSharedRef<ITableRow> GenerateWeaponListRow(UClass* WeaponType, const TSharedRef<STableViewBase>& OwningList);
	FReply WeaponPriorityUp();
	FReply WeaponPriorityDown();

	virtual FReply OnButtonClick(uint16 ButtonID);
	FReply OKClick();
	FReply CancelClick();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObjects(WeaponList);
	}
};
#endif