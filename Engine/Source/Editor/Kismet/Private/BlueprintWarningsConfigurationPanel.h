// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintSupport.h"
#include "BlueprintRuntimeSettings.h"

typedef TSharedPtr<FBlueprintWarningDeclaration> FBlueprintWarningListEntry;
typedef SListView<FBlueprintWarningListEntry> FBlueprintWarningListView;
class SSettingsEditorCheckoutNotice;

class SBlueprintWarningsConfigurationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlueprintWarningsConfigurationPanel){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
private:
	friend class SBlueprintWarningRow;

	void UpdateSelectedWarningBehaviors(EBlueprintWarningBehavior NewBehavior, const FBlueprintWarningDeclaration& AlteredWarning );

	// SListView requires TArray<TSharedRef<FFoo>> so we cache off a list from core:
	TArray<TSharedPtr<FBlueprintWarningDeclaration>> CachedBlueprintWarningData;
	// Again, SListView boilerplate:
	TArray<TSharedPtr<EBlueprintWarningBehavior>> CachedBlueprintWarningBehaviors;
	// Storing the listview, so we can apply updates to all selected entries:
	TSharedPtr< FBlueprintWarningListView > ListView;
	// Stored so that we can only enable controls when the settings files is writable:
	TSharedPtr<SSettingsEditorCheckoutNotice> SettingsEditorCheckoutNotice;
};

