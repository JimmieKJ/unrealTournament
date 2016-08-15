// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUTWeaponConfigDialog.h"
#include "../Base/SUTDialogBase.h"

#if !UE_SERVER

struct  FWeaponListEntry
{
	TWeakObjectPtr<AUTWeapon> Weapon;
	FWeaponInfo* WeaponInfoPtr;
	TSharedPtr<SVerticalBox> ButtonBox;

	FWeaponListEntry()
		: Weapon(nullptr)
		, WeaponInfoPtr(nullptr)
	{
	}

	FWeaponListEntry(TWeakObjectPtr<AUTWeapon> inWeapon, FWeaponInfo* inWeaponInfoPtr)
		: Weapon(inWeapon)
		, WeaponInfoPtr(inWeaponInfoPtr)
	{
	}

	static TSharedRef<FWeaponListEntry> Make(TWeakObjectPtr<AUTWeapon> inWeapon, FWeaponInfo* inWeaponInfoPtr)
	{
		return MakeShareable(new FWeaponListEntry(inWeapon, inWeaponInfoPtr));
	}
};

class UNREALTOURNAMENT_API SUTWeaponPriorityDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTWeaponPriorityDialog)
	: _DialogTitle(NSLOCTEXT("SUTWeaponPriorityDialog","Title","Auto-Switch Priorities"))
	, _DialogSize(FVector2D(800, 1000))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	, _ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)			
	SLATE_ARGUMENT(FText, DialogTitle)											
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_ARGUMENT(uint16, ButtonMask)
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void InitializeList(TArray<FWeaponInfo>& AllWeapons);

	float GetPriorityFor(TWeakObjectPtr<AUTWeapon> Weapon);

protected:

	// Holds a list of all of the weapon data...
	TArray<TSharedPtr<FWeaponListEntry>> WeaponList;


	TSharedPtr< SListView<TSharedPtr<FWeaponListEntry>>> WeaponListView;

	TSharedRef<ITableRow> GenerateWeaponListRow(TSharedPtr<FWeaponListEntry> Item, const TSharedRef<STableViewBase>& OwningList);
	void OnWeaponListSelectionChanged(TSharedPtr<FWeaponListEntry> Item, ESelectInfo::Type SelectInfo);

	FReply MoveUpClicked(TSharedPtr<FWeaponListEntry> Item);
	FReply MoveTopClicked(TSharedPtr<FWeaponListEntry> Item);

	FReply MoveDownClicked(TSharedPtr<FWeaponListEntry> Item);
	FReply MoveBottomClicked(TSharedPtr<FWeaponListEntry> Item);
};

#endif