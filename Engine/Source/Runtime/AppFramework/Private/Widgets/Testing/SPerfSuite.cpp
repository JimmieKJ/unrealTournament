// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AppFrameworkPrivatePCH.h"
#include "SPerfSuite.h"
#include "STableViewTesting.h"
#include "ISlateReflectorModule.h"

void SummonPerfTestSuite()
{
	FSlateApplication::Get().AddWindow
	( 
		SNew(SWindow)
		.IsInitiallyMaximized(false)
		.ScreenPosition( FVector2D(0,0) )
		.ClientSize( FVector2D(1920,1200) )
		[
			MakeTableViewTesting()
		]
	);

	FSlateApplication::Get().AddWindow
	(
		SNew(SWindow)
		.ClientSize(FVector2D(640,800))
		[
			FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").GetWidgetReflector()
		]
	);
}
