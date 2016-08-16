// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUTWeaponConfigDialog.h"
#include "../Base/SUTDialogBase.h"

#if !UE_SERVER

class UNREALTOURNAMENT_API SUTWeaponWheelConfigDialog : public SUTDialogBase
{
public:
	SLATE_BEGIN_ARGS(SUTWeaponWheelConfigDialog)
	: _DialogTitle(NSLOCTEXT("SUTWeaponWheelConfig","Title","Configure the Weapon Wheel"))
	, _DialogSize(FVector2D(1300, 850))
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
	void InitializeList(TArray<FWeaponInfo>* inAllWeapons);
	virtual FReply OnButtonClick(uint16 ButtonID) override;

protected:

	TSharedPtr<SCanvas> ButtonCanvas;
	TArray<FWeaponInfo>* AllWeapons;
	TArray<FSlateDynamicImageBrush*> SlotIcons;

	const FSlateBrush* GetWeaponImage(int32 Slot) const;
	TArray<TSharedPtr<SUTButton>> WheelButtons;
	TArray<TSharedPtr<STextBlock>> WheelNumbers;
	FText GetGroupNumber(int32 Slot) const;

	void GenerateSlots();
	int32 SelectedSlot;

	TArray<int32> LocalSlots;

	TArray<TSharedPtr<SUTButton>> WeaponGroupButtons;
	TSharedPtr<SGridPanel> WeaponGroupPanel;
	void GenerateWeaponGroups();
	FReply WeaponGroupSelected(int32 GroupIndex);
	FReply WheelClicked(int32 SlotIndex);
};

#endif