// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "CompareScreenshotsCommandlet.h"
#include "Modules/ModuleManager.h"

#include "Interfaces/IScreenShotToolsModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogCompareScreenshotsCommandlet, Log, All);

UCompareScreenshotsCommandlet::UCompareScreenshotsCommandlet(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UCompareScreenshotsCommandlet::Main(FString const& Params)
{
	FString ScreenshotExportPath;
	if ( !FParse::Value(*Params, TEXT("ExportPath="), ScreenshotExportPath) )
	{
		UE_LOG(LogCompareScreenshotsCommandlet, Error, TEXT("Must specify an export location, e.g. ExportPath=D:/ExportedComparison"));
		return 1;
	}

	IScreenShotToolsModule& ScreenShotModule = FModuleManager::LoadModuleChecked<IScreenShotToolsModule>("ScreenShotComparisonTools");
	IScreenShotManagerPtr ScreenshotManager = ScreenShotModule.GetScreenShotManager();

	ScreenshotManager->CompareScreensotsAsync().Wait();
	ScreenshotManager->ExportScreensotsAsync(ScreenshotExportPath).Wait();

	return 0;
}
