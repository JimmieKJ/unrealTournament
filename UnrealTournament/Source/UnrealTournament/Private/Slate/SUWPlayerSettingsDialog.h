// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "SUWDialog.h"

class SUWPlayerSettingsDialog : public SUWDialog
{
public:
	void Construct(const FArguments& InArgs);
protected:

	TSharedPtr<SEditableTextBox> PlayerName;
	TSharedPtr<SCheckBox> AutoWeaponSwitch;
	TArray<UClass*> WeaponList;
	TSharedPtr< SListView<UClass*> > WeaponPriorities;
	TSharedPtr<SSlider> WeaponBobScaling, ViewBobScaling;

	FReply OKClick();
	FReply CancelClick();
	FReply WeaponPriorityUp();
	FReply WeaponPriorityDown();
	void OnNameTextChanged(const FText& NewText);
	TSharedRef<ITableRow> GenerateWeaponListRow(UClass* WeaponType, const TSharedRef<STableViewBase>& OwningList);
	// find/load weapon class and add to WeaponList
	void AddWeapon(const FString& WeaponPath);
};