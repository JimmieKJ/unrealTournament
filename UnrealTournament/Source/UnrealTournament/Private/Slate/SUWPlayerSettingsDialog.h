// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Slate.h"
#include "SUWDialog.h"

class SUWPlayerSettingsDialog : public SUWDialog, public FGCObject
{
public:
	void Construct(const FArguments& InArgs);
protected:

	/** library of weapon classes pulled from asset registry */
	UObjectLibrary* WeaponLibrary;

	TSharedPtr<SEditableTextBox> PlayerName;
	TSharedPtr<SCheckBox> AutoWeaponSwitch;
	TArray<UClass*> WeaponList;
	TSharedPtr< SListView<UClass*> > WeaponPriorities;
	TSharedPtr<SSlider> WeaponBobScaling, ViewBobScaling;
	FLinearColor SelectedPlayerColor;

	FReply OKClick();
	FReply CancelClick();
	FReply WeaponPriorityUp();
	FReply WeaponPriorityDown();
	FReply PlayerColorClicked(const FGeometry& Geometry, const FPointerEvent& Event);
	FLinearColor GetSelectedPlayerColor() const
	{
		return SelectedPlayerColor;
	}
	void PlayerColorChanged(FLinearColor NewValue);
	void OnNameTextChanged(const FText& NewText);
	TSharedRef<ITableRow> GenerateWeaponListRow(UClass* WeaponType, const TSharedRef<STableViewBase>& OwningList);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(WeaponLibrary);
	}
};