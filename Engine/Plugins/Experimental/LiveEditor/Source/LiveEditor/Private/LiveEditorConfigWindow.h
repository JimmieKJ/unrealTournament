// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "portmidi.h"

//////////////////////////////////////////////////////////////////////////
// SLiveEditorConfigWindow

class SLiveEditorConfigWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLiveEditorConfigWindow){}
	SLATE_END_ARGS()

public:
	FReply AddBlueprintToList();
	FReply RefreshDevices();

	void Construct(const FArguments& InArgs);
	void RefreshBlueprintTemplates();
	void WizardActivated();

	void ConfigureDevice( PmDeviceID DeviceID );

private:
	EVisibility ShowDeviceConfigWindow() const;
	TSharedRef<ITableRow> OnGenerateWidgetForBlueprintList(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateWidgetForDeviceList(TSharedPtr<PmDeviceID> Item, const TSharedRef<STableViewBase>& OwnerTable);

	TArray< TSharedPtr<FString> > SharedBlueprintTemplates;
	TSharedPtr< SListView< TSharedPtr<FString> > > BlueprintListView;

	TArray< TSharedPtr<PmDeviceID> > SharedDeviceIDs;
	TSharedPtr< SListView< TSharedPtr<PmDeviceID> > > DeviceListView;

	TSharedPtr< class SLiveEditorWizardWindow > LiveEditorWizardWindow;
};
