// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"

/**
 * Either a button to directly install or a hyperlink to a website to download the suggested IDE for the platform.
 * Only visible when no compiler is available.
 */
class SGetSuggestedIDEWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGetSuggestedIDEWidget)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	
private:
	/** Creates the appropriate widget to display for the platform */
	TSharedRef<SWidget> CreateGetSuggestedIDEWidget() const;

	/** Gets the visibility of the global error label IDE Link */
	EVisibility GetVisibility() const;

	/** Handler for when the error label IDE hyperlink was clicked */
	void OnDownloadIDEClicked(FString URL);

	/** Handler for when the install IDE button was clicked */
	FReply OnInstallIDEClicked();

	/** Handler for when the suggested IDE installer has finished downloading */
	void OnIDEInstallerDownloadComplete(bool bWasSuccessful);

private:

	/** Handle to the notification displayed when downloading an IDE installer */
	TSharedPtr<class SNotificationItem> IDEDownloadNotification;

};