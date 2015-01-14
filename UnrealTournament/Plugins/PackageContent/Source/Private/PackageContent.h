// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LevelEditor.h"

class FPackageContent : public TSharedFromThis< FPackageContent >
{
public:
	~FPackageContent();

	static TSharedRef< FPackageContent > Create();

private:
	FPackageContent(const TSharedRef< FUICommandList >& InActionList);
	void Initialize();


	void OpenPackageLevelWindow();
	void OpenPackageWeaponWindow();
	void OpenPackageHatWindow();

	void CreatePackageContentMenu(FToolBarBuilder& Builder);
	TSharedRef<FExtender> OnExtendLevelEditorViewMenu(const TSharedRef<FUICommandList> CommandList);
	TSharedRef< SWidget > GenerateOpenPackageMenuContent();

	TSharedRef< FUICommandList > ActionList;

	TSharedPtr< FExtender > MenuExtender;

	void CreateUATTask(const FString& CommandLine, const FText& TaskName, const FText &TaskShortName, const FSlateBrush* TaskIcon);

	struct EventData
	{
		FString EventName;
		double StartTime;
	};

	// Handles clicking the packager notification item's Cancel button.
	static void HandleUatCancelButtonClicked(TSharedPtr<FMonitoredProcess> PackagerProcess);

	// Handles clicking the hyper link on a packager notification item.
	static void HandleUatHyperlinkNavigate();

	// Handles canceled packager processes.
	static void HandleUatProcessCanceled(TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName, EventData Event);

	// Handles the completion of a packager process.
	static void HandleUatProcessCompleted(int32 ReturnCode, TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName, EventData Event);

	// Handles packager process output.
	static void HandleUatProcessOutput(FString Output, TWeakPtr<class SNotificationItem> NotificationItemPtr, FText TaskName);
};