// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"

class ULocalizationTarget;
class SLocalizationDashboard;

class FLocalizationDashboard
{
public:
	static FLocalizationDashboard* Get();
	static void Initialize();
	static void Terminate();
	void Show();
	TWeakPtr<SDockTab> ShowTargetEditorTab(ULocalizationTarget* const LocalizationTarget);

private:
	FLocalizationDashboard();
	~FLocalizationDashboard();
	void RegisterTabSpawner();
	void UnregisterTabSpawner();
	
private:
	static const FName TabName;
	static FLocalizationDashboard* Instance;
	TSharedPtr<SLocalizationDashboard> LocalizationDashboardWidget;
};